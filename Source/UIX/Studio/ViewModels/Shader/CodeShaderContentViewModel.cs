﻿// 
// The MIT License (MIT)
// 
// Copyright (c) 2023 Miguel Petersen
// Copyright (c) 2023 Advanced Micro Devices, Inc
// Copyright (c) 2023 Avalanche Studios Group
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

using System;
using System.Collections.ObjectModel;
using System.Reactive;
using System.Reactive.Subjects;
using System.Windows.Input;
using Avalonia;
using Avalonia.Media;
using DynamicData;
using DynamicData.Binding;
using ReactiveUI;
using Runtime.ViewModels.Shader;
using Studio.Models.Workspace.Objects;
using Studio.Services;
using Studio.ViewModels.Documents;
using Studio.ViewModels.Workspace.Listeners;
using Studio.ViewModels.Workspace.Properties;

namespace Studio.ViewModels.Shader
{
    public class CodeShaderContentViewModel : ReactiveObject, IShaderContentViewModel
    {
        /// <summary>
        /// Given descriptor
        /// </summary>
        public ShaderDescriptor? Descriptor
        {
            set
            {
                NavigationLocation = null;
                NavigationLocation = value?.StartupLocation;
            }
        }

        /// <summary>
        /// View icon
        /// </summary>
        public StreamGeometry? Icon
        {
            get => _icon;
            set => this.RaiseAndSetIfChanged(ref _icon, value);
        }
        
        /// <summary>
        /// Workspace within this overview
        /// </summary>
        public IPropertyViewModel? PropertyCollection
        {
            get => _propertyCollection;
            set => this.RaiseAndSetIfChanged(ref _propertyCollection, value);
        }

        /// <summary>
        /// Selection command
        /// </summary>
        public ICommand? OnSelected { get; }

        /// <summary>
        /// Is this model active?
        /// </summary>
        public bool IsActive
        {
            get => _isActive;
            set => this.RaiseAndSetIfChanged(ref _isActive, value);
        }

        /// <summary>
        /// Current location
        /// </summary>
        public ShaderLocation? NavigationLocation
        {
            get => _navigationLocation;
            set => this.RaiseAndSetIfChanged(ref _navigationLocation, value);
        }

        /// <summary>
        /// Underlying object
        /// </summary>
        public Workspace.Objects.ShaderViewModel? Object
        {
            get => _object;
            set
            {
                this.RaiseAndSetIfChanged(ref _object, value);

                if (_object != null)
                {
                    OnObjectChanged();
                }
            }
        }

        /// <summary>
        /// Selected file
        /// </summary>
        public ShaderFileViewModel? SelectedShaderFileViewModel
        {
            get => _selectedSelectedShaderFileViewModel;
            set => this.RaiseAndSetIfChanged(ref _selectedSelectedShaderFileViewModel, value);
        }

        public CodeShaderContentViewModel()
        {
            OnSelected = ReactiveCommand.Create(OnParentSelected);
        }

        /// <summary>
        /// Invoked on parent selection
        /// </summary>
        private void OnParentSelected()
        {
            if (App.Locator.GetService<IWorkspaceService>() is { } service)
            {
                // Create navigation vm
                service.SelectedShader = new ShaderNavigationViewModel()
                {
                    Shader = Object,
                    SelectedFile = _selectedSelectedShaderFileViewModel
                };

                // Bind selection
                service.SelectedShader.WhenAnyValue(x => x.SelectedFile)
                    .Subscribe(x => SelectedShaderFileViewModel = x);
            }
        }

        /// <summary>
        /// Invoked on object change
        /// </summary>
        private void OnObjectChanged()
        {
            // Submit request if not already
            if (Object!.Contents == string.Empty)
            {
                PropertyCollection?.GetService<IShaderCodeService>()?.EnqueueShaderContents(Object);
            }

            // Set VM when available
            Object.FileViewModels.ToObservableChangeSet().OnItemAdded(x =>
            {
                if (SelectedShaderFileViewModel == null)
                {
                    SelectedShaderFileViewModel = x;
                }
            }).Subscribe();
        }

        /// <summary>
        /// Internal object
        /// </summary>
        private Workspace.Objects.ShaderViewModel? _object;

        /// <summary>
        /// Underlying view model
        /// </summary>
        private IPropertyViewModel? _propertyCollection;

        /// <summary>
        /// Internal icon state
        /// </summary>
        private StreamGeometry? _icon = ResourceLocator.GetIcon("Code");

        /// <summary>
        /// Selected file
        /// </summary>
        private ShaderFileViewModel? _selectedSelectedShaderFileViewModel = null;

        /// <summary>
        /// Internal location
        /// </summary>
        private ShaderLocation? _navigationLocation;

        /// <summary>
        /// Internal active state
        /// </summary>
        private bool _isActive = false;
    }
}