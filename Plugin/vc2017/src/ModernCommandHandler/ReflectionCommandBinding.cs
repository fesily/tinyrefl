/***************************************************************************

Copyright (c) Microsoft Corporation. All rights reserved.
THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.

***************************************************************************/

using Microsoft.VisualStudio.Editor.Commanding;
using System;
using System.ComponentModel.Composition;

namespace ModernCommandHandler
{
    public class ReflectionCommandBinding
    {
        private const int ReflectionCommandId = 0x0100;
        private const string ReflectionCommandSet = "4fd0ea18-9f33-43da-ace0-e387656e584c";

        [Export]
        [CommandBinding(ReflectionCommandSet, ReflectionCommandId, typeof(ReflectionCommandArgs))]
        internal CommandBindingDefinition reflectionCommandBinding;
    }
}
