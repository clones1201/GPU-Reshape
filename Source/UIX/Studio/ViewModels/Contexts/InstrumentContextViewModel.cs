﻿using System.Collections.ObjectModel;
using System.Reactive;
using System.Windows.Input;
using Message.CLR;
using ReactiveUI;
using Studio.ViewModels.Workspace.Properties;
using Studio.ViewModels.Traits;

namespace Studio.ViewModels.Contexts
{
    public class InstrumentContextViewModel : ReactiveObject, IInstrumentContextViewModel
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
                IsVisible = _targetViewModel is IInstrumentableObject;
            }
        }

        /// <summary>
        /// Display header of this context model
        /// </summary>
        public string Header { get; set; } = "Instrument";
        
        /// <summary>
        /// All items within this context model
        /// </summary>
        public ObservableCollection<IContextMenuItemViewModel> Items { get; } = new();

        /// <summary>
        /// Target command
        /// </summary>
        public ICommand? Command { get; } = null;

        public InstrumentContextViewModel()
        {
            // Standard objects
            Items.Add(new InstrumentNoneContextViewModel());
            Items.Add(new InstrumentAllContextViewModel());
        }

        /// <summary>
        /// Is this context enabled?
        /// </summary>
        public bool IsVisible
        {
            get => _isVisible;
            set => this.RaiseAndSetIfChanged(ref _isVisible, value);
        }

        /// <summary>
        /// Internal enabled state
        /// </summary>
        private bool _isVisible = false;

        /// <summary>
        /// Internal target view model
        /// </summary>
        private object? _targetViewModel;
    }
}