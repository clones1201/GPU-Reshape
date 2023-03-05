﻿using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using DynamicData;
using ReactiveUI;
using Studio.ViewModels.Workspace.Objects;

namespace Studio.ViewModels.Workspace.Properties
{
    public class ShaderCollectionViewModel : ReactiveObject, IShaderCollectionViewModel
    {
        /// <summary>
        /// Name of this property
        /// </summary>
        public string Name { get; } = "Shaders";

        /// <summary>
        /// Visibility of this property
        /// </summary>
        public PropertyVisibility Visibility => PropertyVisibility.WorkspaceTool;

        /// <summary>
        /// Parent property
        /// </summary>
        public IPropertyViewModel? Parent { get; set; }

        /// <summary>
        /// Child properties
        /// </summary>
        public ISourceList<IPropertyViewModel> Properties { get; set; } = new SourceList<IPropertyViewModel>();

        /// <summary>
        /// All services
        /// </summary>
        public ISourceList<IPropertyService> Services { get; set; } = new SourceList<IPropertyService>();

        /// <summary>
        /// All shaders within this collection
        /// </summary>
        public ObservableCollection<Objects.ShaderViewModel> Shaders { get; } = new();
        
        /// <summary>
        /// View model associated with this property
        /// </summary>
        public IConnectionViewModel? ConnectionViewModel
        {
            get => _connectionViewModel;
            set
            {
                this.RaiseAndSetIfChanged(ref _connectionViewModel, value);

                OnConnectionChanged();
            }
        }

        /// <summary>
        /// Add a new shader to this collection
        /// </summary>
        /// <param name="shaderViewModel"></param>
        public void AddShader(Objects.ShaderViewModel shaderViewModel)
        {
            // Flat view
            Shaders.Add(shaderViewModel);
            
            // Create lookup
            _shaderModelGUID.Add(shaderViewModel.GUID, shaderViewModel);
        }

        /// <summary>
        /// Get a shader from this collection
        /// </summary>
        /// <param name="GUID"></param>
        /// <returns>null if not found</returns>
        public Objects.ShaderViewModel? GetShader(UInt64 GUID)
        {
            _shaderModelGUID.TryGetValue(GUID, out Objects.ShaderViewModel? shaderViewModel);
            return shaderViewModel;
        }

        /// <summary>
        /// Get a shader from this collection, add if not found
        /// </summary>
        /// <param name="GUID"></param>
        /// <returns></returns>
        public Objects.ShaderViewModel GetOrAddShader(UInt64 GUID)
        {
            Objects.ShaderViewModel? shaderViewModel = GetShader(GUID);
            
            // Create if not found
            if (shaderViewModel == null)
            {
                shaderViewModel = new Objects.ShaderViewModel()
                {
                    GUID = GUID
                };
                
                // Append
                AddShader(shaderViewModel);
            }

            // OK
            return shaderViewModel;
        }

        /// <summary>
        /// Invoked when a connection has changed
        /// </summary>
        private void OnConnectionChanged()
        {
            // Set connection
            _shaderCodeService.ConnectionViewModel = ConnectionViewModel;
            
            // Make visible
            Parent?.Services.Add(_shaderCodeService);
            
            // Register internal listeners
            _connectionViewModel?.Bridge?.Register(_shaderCodeService);
            
            // Create all properties
            CreateProperties();
        }

        /// <summary>
        /// Create all properties
        /// </summary>
        private void CreateProperties()
        {
            Properties.Clear();

            if (_connectionViewModel == null)
            {
                return;
            }
        }

        /// <summary>
        /// GUID to shader mappings
        /// </summary>
        private Dictionary<UInt64, Objects.ShaderViewModel> _shaderModelGUID = new();

        /// <summary>
        /// Code listener
        /// </summary>
        private Listeners.ShaderCodeService _shaderCodeService = new();
        
        /// <summary>
        /// Internal connection
        /// </summary>
        private IConnectionViewModel? _connectionViewModel;
    }
}