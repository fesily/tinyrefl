/***************************************************************************

Copyright (c) Microsoft Corporation. All rights reserved.
THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.

***************************************************************************/

using System;
using System.Collections;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Resources;
using System.Text;
using EnvDTE;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.VCProjectEngine;
using Process = System.Diagnostics.Process;
using Thread = System.Threading.Thread;

namespace ReflectionCommandImplementation
{
    public static class Reflection
    {
        private static string _assemblePath;
        public static bool IsInit { get; private set; }

        private static IVsOutputWindowPane _outputWindow;

        public static bool InitEnvInternal()
        {
            _outputWindow = Package.GetGlobalService(typeof(SVsGeneralOutputWindowPane)) as IVsOutputWindowPane;
            if (_outputWindow == null)
                return false;
            try
            {
                string dir = null;
#if USEAssemble
                var assemble = AppDomain.CurrentDomain.GetAssemblies()
                    .FirstOrDefault(s => s.FullName.Contains("TinyCxxReflection"));
                if (assemble == null)
                {
                    dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                    assemble = Assembly.LoadFile(Path.Combine(
                        dir ?? throw new Exception("can't find plugin assemble path"), "TinyCxxReflection.dll"));
                }
                else
                {
                    dir = Path.GetDirectoryName(assemble.Location);
                }
#else
                var assemble = Assembly.GetExecutingAssembly();
                dir = Path.GetDirectoryName(assemble.Location);
#endif
                _assemblePath = dir ?? throw new Exception("can't find plugin assemble path");
                var existPath = Path.Combine(dir, "tinyrefl_extract");
                if (File.Exists(existPath))
                    return true;

                var names = assemble.GetManifestResourceNames();
                var first = names.FirstOrDefault(s => s.Contains("Resource.resources"));
                if (first == null) throw new Exception("can't load plugin assemble resources");
                var stream = assemble.GetManifestResourceStream(first);
                if (stream == null) throw new Exception("can't load plugin assemble resources stream");
                var rs = new ResourceSet(new ResourceReader(stream));
                var buf = (byte[]) rs.GetObject("tinyrefl");
                if (buf == null) throw new Exception("can't load plugin assemble tinyrefl resources ");
                var zipPath = Path.Combine(dir, "tinyrefl.zip");
                File.WriteAllBytes(zipPath, buf);
                File.Create(existPath).Close();
                ZipFile.ExtractToDirectory(zipPath, dir, Encoding.ASCII);
                File.Delete(zipPath);
                return true;
            }
            catch (Exception e)
            {
                _outputWindow.OutputStringThreadSafe($"tinyrefl init faild:{e.Message}");
                // ignored
                return false;
            }
        }

        public static void InitEnv()
        {
            var thread = new Thread(() => { IsInit = InitEnvInternal(); });
            thread.Start();
        }

        private static string GetDocPath(ITextView textView)
        {
            ITextDocument document;
            var properties = textView.TextDataModel.DocumentBuffer.Properties;
            if (properties.TryGetProperty(typeof(ITextDocument),
                out document))
                return document.FilePath;

            return "";
        }

        public static bool IsSupport(ITextView textView)
        {
            try
            {
                if (IsInit)
                {
                    var path = GetDocPath(textView);
                    if (path != "")
                    {
                        var extension = Path.GetExtension(path);
                        if (extension == ".h" || extension == ".hpp")
                            return true;
                    }
                }

                return false;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public static void ExecuteReflection(ITextView textView)
        {
            if (!IsSupport(textView)) return;
            var engine = Package.GetGlobalService(typeof(SVCProjectEngine)) as VCProjectEngine;
            if (engine == null || _outputWindow == null) return;

            if (!GetReflectionArguments(textView, _outputWindow, out var arguments))
                _outputWindow.OutputStringThreadSafe("can't find project includes and defines");

            arguments +=
                $" -msvc=1 -std=c++17 -nodry=0 -cl=\"{Path.Combine(_assemblePath, "clang.exe")}\" -clv=6.0.0 -SI=\"{Path.Combine(_assemblePath, "/clang/6.0.0/include")}\" \"{GetDocPath(textView)}\"";
            ThreadHelper.JoinableTaskFactory.RunAsync(async () =>
            {
                try
                {
                    var info = new ProcessStartInfo
                    {
                        Arguments = arguments,
                        WorkingDirectory = _assemblePath,
                        FileName = Path.Combine(_assemblePath, "tinyrefl.exe"),
                        UseShellExecute = false,
                        CreateNoWindow = true,
                        RedirectStandardError = true,
                        RedirectStandardOutput = true
                    };
                    var process = new Process
                    {
                        StartInfo = info
                    };
                    process.ErrorDataReceived +=
                        (sender, arg) => _outputWindow.OutputStringThreadSafe($"{arg.Data}\r\n");
                    process.OutputDataReceived +=
                        (sender, arg) => _outputWindow.OutputStringThreadSafe($"{arg.Data}\r\n");
                    var ret = process.Start();
                    process.BeginErrorReadLine();
                    process.BeginOutputReadLine();
                    ret &= await process.WaitForExitAsync() == 0;
                    _outputWindow.OutputStringThreadSafe($"generate reflection {(ret ? "success" : "fail")}!\r\n");
                }
                catch (Exception e)
                {
                    _outputWindow.OutputStringThreadSafe(e.Message);
                }
            });
        }

        private static bool GetReflectionArguments(ITextView textView, IVsOutputWindowPane outputWindow,
            out string arguments)
        {
            arguments = string.Empty;
            try
            {
                IVsPersistDocData docData;
                if (!textView.TextDataModel.DocumentBuffer.Properties.TryGetProperty(typeof(IVsPersistDocData),
                        out docData) ||
                    docData == null) throw new Exception("can't find document data from text data model");

                var type = docData.GetType().BaseType;
                if (type == null) throw new Exception("can't find TextDocData Type");

                var propertyInfo = type.GetProperty("DTEDocument", BindingFlags.NonPublic | BindingFlags.Instance);
                if (propertyInfo == null) throw new Exception("can't find TextDocData DTEDocument property");

                var method = propertyInfo.GetMethod;
                var document = method.Invoke(docData, null) as Document;
                if (document == null) throw new Exception("can't get TextDocData EnvDTE.DTEDocument");
                // finder file in which project
                var project = document.ProjectItem.ContainingProject.Object as VCProject;
                if (project == null)
                {
                    var dte = Package.GetGlobalService(typeof(SDTE)) as DTE;
                    if (dte != null)
                    {
                        var activeProjects = dte.ActiveSolutionProjects as Array;
                        if (activeProjects != null && activeProjects.Length > 0)
                            project = activeProjects.GetValue(0) as VCProject;
                    }
                }
                if (project == null) throw new Exception("can't get VCProject from ContainingProject");

                if (!(project.ActiveConfiguration is VCConfiguration2 activeConfiguration)) return false;

                if (!(activeConfiguration.Platform is VCPlatform)) return false;
                // find cl_compiler_tool object
                if (!(((IEnumerable) activeConfiguration.Tools).Cast<object>()
                    .FirstOrDefault(finder => finder is VCCLCompilerTool) is VCCLCompilerTool clCompilerTool))
                    return false;

                var includeDirs =
                    clCompilerTool.FullIncludePath.Split(new[] {';'}, StringSplitOptions.RemoveEmptyEntries);
                var preprocessorDefinitions =
                    clCompilerTool.PreprocessorDefinitions.Split(new[] {';'}, StringSplitOptions.RemoveEmptyEntries);
                foreach (var includeDir in includeDirs)
                    arguments += $" -I=\"{includeDir}\" ";

                foreach (var preprocessorDefinition in preprocessorDefinitions)
                    arguments += $" -D={preprocessorDefinition} ";
                return true;
            }
            catch (Exception e)
            {
                outputWindow.OutputStringThreadSafe(e.Message);
                return false;
            }
        }
    }
}