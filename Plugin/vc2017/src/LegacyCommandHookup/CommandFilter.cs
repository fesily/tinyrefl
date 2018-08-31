/***************************************************************************

Copyright (c) Microsoft Corporation. All rights reserved.
THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.

***************************************************************************/

using ReflectionCommandImplementation;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.TextManager.Interop;
using System;

namespace LegacyCommandHandler
{
    internal class CommandFilter : IOleCommandTarget
    {
        private readonly ITextView textView;
        private readonly CommandFilterHookup contextProvider;

        public CommandFilter(IVsTextView textViewAdapter, ITextView textView, CommandFilterHookup contextProvider)
        {
            this.textView = textView;
            this.contextProvider = contextProvider;
            textViewAdapter.AddCommandFilter(this, out var nextFilter);
            this.NextTarget = nextFilter;
        }

        public IOleCommandTarget NextTarget { get; set; }

        /// <summary>
        /// Command ID.
        /// </summary>
        public const int ReflectionCommandId = 0x0100;

        /// <summary>
        /// Command menu group (command set GUID).
        /// </summary>
        public static readonly Guid ReflectionCommandSet = new Guid("4fd0ea18-9f33-43da-ace0-e387656e584c");

        public int QueryStatus(ref Guid pguidCmdGroup, uint cCmds, OLECMD[] prgCmds, IntPtr pCmdText)
        {
            if (pguidCmdGroup == ReflectionCommandSet && cCmds == 1 && prgCmds[0].cmdID == ReflectionCommandId)
            {
                if (Reflection.IsSupport(textView))
                {
                    prgCmds[0].cmdf = (uint)(OLECMDF.OLECMDF_ENABLED | OLECMDF.OLECMDF_SUPPORTED);
                }
                return VSConstants.S_OK;
            }

            return NextTarget.QueryStatus(ref pguidCmdGroup, cCmds, prgCmds, pCmdText);
        }

        public int Exec(ref Guid pguidCmdGroup, uint nCmdID, uint nCmdexecopt, IntPtr pvaIn, IntPtr pvaOut)
        {
            if (pguidCmdGroup == ReflectionCommandSet && nCmdID == ReflectionCommandId)
            {
                Reflection.ExecuteReflection(textView);
                return VSConstants.S_OK;
            }

            return NextTarget.Exec(ref pguidCmdGroup, nCmdID, nCmdexecopt, pvaIn, pvaOut);
        }
    }
}
