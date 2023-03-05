﻿using System.Collections.ObjectModel;
using System.Windows.Input;
using Message.CLR;
using ReactiveUI;
using Runtime.Models.Objects;
using Runtime.ViewModels.Workspace.Properties;
using Studio.Models.Workspace;
using Studio.ViewModels.Contexts;
using Studio.ViewModels.Traits;
using Studio.ViewModels.Workspace.Properties;

namespace GRS.Features.Initialization.UIX.Contexts
{
    public class InitializationContextMenuItemViewModel : ReactiveObject, IContextMenuItemViewModel
    {
        /// <summary>
        /// Target view model of the context
        /// </summary>
        public object? TargetViewModel
        {
            get => _targetViewModel;
            set
            {
                this.RaiseAndSetIfChanged(ref _targetViewModel, value);
                
                // Enabled if resource bounds is available
                _featureInfo = (_targetViewModel as IInstrumentableObject)?
                    .GetWorkspace()?
                    .GetProperty<IFeatureCollectionViewModel>()?
                    .GetFeature("Initialization");
                
                IsVisible = _featureInfo.HasValue;
            }
        }

        /// <summary>
        /// Display header of this context model
        /// </summary>
        public string Header { get; set; } = "Initialization";
        
        /// <summary>
        /// All items within this context model
        /// </summary>
        public ObservableCollection<IContextMenuItemViewModel> Items { get; } = new();

        /// <summary>
        /// Context command
        /// </summary>
        public ICommand Command { get; }

        /// <summary>
        /// Is this context enabled?
        /// </summary>
        public bool IsVisible
        {
            get => _isEnabled;
            set => this.RaiseAndSetIfChanged(ref _isEnabled, value);
        }

        public InitializationContextMenuItemViewModel()
        {
            Command = ReactiveCommand.Create(OnInvoked);
        }

        /// <summary>
        /// Command implementation
        /// </summary>
        private void OnInvoked()
        {
            if (_targetViewModel is not IInstrumentableObject instrumentable)
            {
                return;
            }

            // Request instrumentation
            instrumentable.InstrumentationState = new InstrumentationState()
            {
                FeatureBitMask = _featureInfo?.FeatureBit ?? 0
            };
        }

        /// <summary>
        /// Queried feature information
        /// </summary>
        private FeatureInfo? _featureInfo;

        /// <summary>
        /// Internal enabled state
        /// </summary>
        private bool _isEnabled = false;

        /// <summary>
        /// Internal target view model
        /// </summary>
        private object? _targetViewModel;
    }
}