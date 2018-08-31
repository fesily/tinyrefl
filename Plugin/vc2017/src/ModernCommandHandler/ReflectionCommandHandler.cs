/***************************************************************************

Copyright (c) Microsoft Corporation. All rights reserved.
THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.

***************************************************************************/

using System;
using ReflectionCommandImplementation;
using Microsoft.VisualStudio.Commanding;
using Microsoft.VisualStudio.Text.Operations;
using Microsoft.VisualStudio.Utilities;
using System.ComponentModel.Composition;
using System.IO;
using System.IO.Compression;
using System.IO.Packaging;
using System.Linq;
using System.Reflection;
using System.Resources;
using System.Security.Permissions;
using System.Text;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.TextManager.Interop;

namespace ModernCommandHandler
{
    [Export(typeof(ICommandHandler))]
    [ContentType("C/C++")]
    [Name(nameof(ReflectionCommandHandler))]
    public class ReflectionCommandHandler : ICommandHandler<ReflectionCommandArgs>
    {
        public string DisplayName => "c++ reflection this header";

        [Import]
        private IEditorOperationsFactoryService EditorOperations = null;
        [Import]
        internal IVsEditorAdaptersFactoryService AdapterService = null;

        private bool IsInit => Reflection.IsInit;

        ReflectionCommandHandler()
        {
            Reflection.InitEnv();
        }


        public CommandState GetCommandState(ReflectionCommandArgs args)
        {
            if (!IsInit) return CommandState.Unavailable;
            // must cpp header
            return Reflection.IsSupport(args.TextView) ? CommandState.Available : CommandState.Unavailable;
        }

        public bool ExecuteCommand(ReflectionCommandArgs args, CommandExecutionContext executionContext)
        {
            using (executionContext.OperationContext.AddScope(false, DisplayName))
            {
                Reflection.ExecuteReflection(args.TextView);
            }
            return true;
        }

    }
}
