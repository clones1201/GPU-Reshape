﻿using System;
using Avalonia;
using DynamicData;
using GRS.Features.ResourceBounds.UIX.Contexts;
using GRS.Features.ResourceBounds.UIX.Workspace;
using Studio.Plugin;
using Studio.Services;
using Studio.ViewModels.Contexts;
using Studio.ViewModels.Traits;
using Studio.ViewModels.Workspace;

namespace GRS.Features.ResourceBounds.UIX
{
    public class Plugin : IPlugin, IWorkspaceExtension
    {
        public PluginInfo Info { get; } = new()
        {
            Name = "ResourceBounds",
            Description = "Provides instrumentation around resource data indexing operations",
            Dependencies = new string[]{ }
        };
        
        /// <summary>
        /// Install this plugin
        /// </summary>
        /// <returns></returns>
        public bool Install()
        {
            // Add to context menus
            AvaloniaLocator.Current.GetService<IContextMenuService>()?.ViewModel
                .GetItem<IInstrumentContextViewModel>()?
                .Items.Add(new ResourceBoundsContextMenuItemViewModel());
            
            // Add workspace extension
            AvaloniaLocator.Current.GetService<IWorkspaceService>()?.Extensions.Add(this);

            // OK
            return true;
        }

        /// <summary>
        /// Uninstall this plugin
        /// </summary>
        public void Uninstall()
        {
            // Remove workspace extension
            AvaloniaLocator.Current.GetService<IWorkspaceService>()?.Extensions.Remove(this);
        }

        /// <summary>
        /// Install an extension
        /// </summary>
        /// <param name="workspaceViewModel"></param>
        public void Install(IWorkspaceViewModel workspaceViewModel)
        {
            // Create service
            workspaceViewModel.PropertyCollection.Services.Add(new ResourceBoundsService(workspaceViewModel));
        }
    }
}
