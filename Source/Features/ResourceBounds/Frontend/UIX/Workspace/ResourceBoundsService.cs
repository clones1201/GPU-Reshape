﻿using System;
using System.Collections.Generic;
using Avalonia.Threading;
using Studio.ViewModels.Workspace;
using Message.CLR;
using GRS.Features.ResourceBounds.UIX.Workspace.Properties.Instrumentation;
using ReactiveUI;
using Runtime.ViewModels.Workspace.Properties;
using Studio.Models.Workspace;
using Studio.ViewModels.Traits;
using Studio.ViewModels.Workspace.Listeners;
using Studio.ViewModels.Workspace.Objects;
using Studio.ViewModels.Workspace.Properties;

namespace GRS.Features.ResourceBounds.UIX.Workspace
{
    public class ResourceBoundsService : IInstrumentationPropertyService, Bridge.CLR.IBridgeListener
    {
        /// <summary>
        /// Feature name
        /// </summary>
        public string Name => "Resource Bounds";
        
        /// <summary>
        /// Parent view model
        /// </summary>
        public IWorkspaceViewModel ViewModel { get; }

        public ResourceBoundsService(IWorkspaceViewModel viewModel)
        {
            ViewModel = viewModel;

            // Add listener to bridge
            viewModel.Connection?.Bridge?.Register(ResourceIndexOutOfBoundsMessage.ID, this);

            // Get properties
            _messageCollectionViewModel = viewModel.PropertyCollection.GetProperty<IMessageCollectionViewModel>();

            // Get services
            _shaderMappingService = viewModel.PropertyCollection.GetService<IShaderMappingService>();
        }

        /// <summary>
        /// Bridge handler
        /// </summary>
        /// <param name="streams"></param>
        /// <param name="count"></param>
        /// <exception cref="NotImplementedException"></exception>
        public void Handle(ReadOnlyMessageStream streams, uint count)
        {
            if (!streams.GetSchema().IsChunked(ResourceIndexOutOfBoundsMessage.ID))
                return;

            var view = new ChunkedMessageView<ResourceIndexOutOfBoundsMessage>(streams);
            
            // Latent update set
            var enqueued = new Dictionary<uint, uint>();
            
            // Preallocate initial latents
            foreach (var kv in _reducedMessages)
            {
                enqueued.Add(kv.Key, 0);
            }

            foreach (ResourceIndexOutOfBoundsMessage message in view)
            {
                // Add to latent set
                if (enqueued.TryGetValue(message.Key, out uint enqueuedCount))
                {
                    enqueued[message.Key] = enqueuedCount + 1u;
                }
                else
                {
                    // Create object
                    var validationObject = new ValidationObject()
                    {
                        Content = $"{(message.Flat.isTexture == 1 ? "Texture" : "Buffer")} {(message.Flat.isWrite == 1 ? "write" : "read")} out of bounds",
                        Count = 1u
                    };
                    
                    // Register with latent
                    enqueued.Add(message.Key, 1u);
                    
                    // Shader view model injection
                    validationObject.WhenAnyValue(x => x.Segment).WhereNotNull().Subscribe(x =>
                    {
                        // Try to get shader collection
                        var shaderCollectionViewModel = ViewModel.PropertyCollection.GetProperty<IShaderCollectionViewModel>();
                        if (shaderCollectionViewModel != null)
                        {
                            // Get the respective shader
                            ShaderViewModel shaderViewModel = shaderCollectionViewModel.GetOrAddShader(x.Location.SGUID);

                            // Append validation object to target shader
                            shaderViewModel.ValidationObjects.Add(validationObject);
                        }
                    });

                    // Enqueue segment binding
                    _shaderMappingService?.EnqueueMessage(validationObject, message.sguid);

                    // Insert lookup
                    _reducedMessages.Add(message.Key, validationObject);

                    // Add to UI visible collection
                    Dispatcher.UIThread.InvokeAsync(() => { _messageCollectionViewModel?.ValidationObjects.Add(validationObject); });
                }

                // Detailed?
                if (message.HasChunk(ResourceIndexOutOfBoundsMessage.Chunk.Detail))
                {
                    ResourceIndexOutOfBoundsMessage.DetailChunk detailChunk = message.GetDetailChunk();

                    // TODO: Optimize the hell out of this, current version is not good enough

                    // Get detailed view model
                    if (!_reducedDetails.TryGetValue(message.Key, out ResourceValidationDetailViewModel? detailViewModel))
                    {
                        // Not found, find the object
                        if (!_reducedMessages.TryGetValue(message.Key, out ValidationObject? validationObject))
                        {
                            continue;
                        }

                        // Create the missing detail view model
                        detailViewModel = new ResourceValidationDetailViewModel();
                        validationObject.DetailViewModel = detailViewModel;
                        
                        // Add lookup
                        _reducedDetails.Add(message.Key, detailViewModel);
                    }

                    // Get resource
                    ResourceValidationObject resourceValidationObject = detailViewModel.FindOrAddResource(detailChunk.token);

                    // Read coordinate
                    uint[] coordinate = detailChunk.coordinate;
                    
                    // Compose detailed message
                    resourceValidationObject.AddUniqueInstance($"Out of bounds at {coordinate[0]}, {coordinate[1]}, {coordinate[2]}");
                }
            }
            
            // Update counts on main thread
            foreach (var kv in enqueued)
            {
                if (_reducedMessages.TryGetValue(kv.Key, out ValidationObject? value))
                {
                    if (kv.Value > 0)
                    {
                        Dispatcher.UIThread.InvokeAsync(() => { value.Count += kv.Value; });
                    }
                }
            }
        }

        /// <summary>
        /// Create an instrumentation property
        /// </summary>
        /// <param name="target"></param>
        /// <returns></returns>
        public IPropertyViewModel? CreateInstrumentationObjectProperty(IPropertyViewModel target)
        {
            // Get feature info on target
            FeatureInfo? featureInfo = (target as IInstrumentableObject)?
                .GetWorkspace()?
                .GetProperty<IFeatureCollectionViewModel>()?
                .GetFeature("Resource Bounds");

            // Invalid or already exists?
            if (featureInfo == null || target.HasProperty<ResourceBoundsPropertyViewModel>())
            {
                return null;
            }

            // Create the property
            return new ResourceBoundsPropertyViewModel()
            {
                Parent = target,
                ConnectionViewModel = target.ConnectionViewModel,
                FeatureInfo = featureInfo.Value
            };
        }

        /// <summary>
        /// All reduced resource messages
        /// </summary>
        private Dictionary<uint, ValidationObject> _reducedMessages = new();

        /// <summary>
        /// All reduced resource messages
        /// </summary>
        private Dictionary<uint, ResourceValidationDetailViewModel> _reducedDetails = new();

        /// <summary>
        /// Shader segment mapper
        /// </summary>
        private IShaderMappingService? _shaderMappingService;

        /// <summary>
        /// Validation container
        /// </summary>
        private IMessageCollectionViewModel? _messageCollectionViewModel;
    }
}