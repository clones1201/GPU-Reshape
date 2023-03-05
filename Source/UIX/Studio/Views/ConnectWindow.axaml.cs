﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Reactive;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Media.Immutable;
using Avalonia.Media.TextFormatting;
using Avalonia.ReactiveUI;
using DynamicData;
using DynamicData.Binding;
using ReactiveUI;
using Studio.Extensions;
using TextFormatter = Runtime.TextFormatter;

namespace Studio.Views
{
    public partial class ConnectWindow : ReactiveWindow<ViewModels.ConnectViewModel>
    {
        public ConnectWindow()
        {
            InitializeComponent();
#if DEBUG
            this.AttachDevTools();
#endif

            // Setup query
            QueryBox.Items = AutoSource;
            QueryBox.TextFilter = QueryTextFilter;
            QueryBox.TextSelector = QuerySelector;
            
            // Set query
            AttributeSegments.QueryBox = QueryBox;
            
            // Bind to VM activator
            this.WhenActivated(disposables => { });
            
            // On data context
            this.WhenAnyValue(x => x.DataContext).CastNullable<ViewModels.ConnectViewModel>().Subscribe(x =>
            {
                // Bind interactions
                x.AcceptClient.RegisterHandler(ctx =>
                {
                    // Request closure
                    Close(true);

                    // OK
                    ctx.SetOutput(true);
                });

                // Set decorators
                AttributeSegments.Decorators = x.QueryDecorators;
            });

            // Bind events
            ConnectionGrid.Events().DoubleTapped
                .ToSignal()
                .Subscribe(x => VM?.Connect.Execute(null));
        }

        /// <summary>
        /// Invoked on filter additions
        /// </summary>
        /// <param name="search">search text</param>
        /// <param name="item">current suggestion</param>
        /// <returns></returns>
        private bool QueryTextFilter(string search, string item)
        {
            VM?.RefreshQuery();
            
            // Do not suggest items already decorated
            if (VM?.QueryDecorators.Items.Any(x => x.Attribute.Key == item.Substring(0, item.Length - 1)) ?? false)
            {
                return false;
            }

            // Compare last word
            string word = search.Split(' ').LastOrDefault() ?? search;
            return item.StartsWith(word.ToLower());
        }

        /// <summary>
        /// Invoked on suggestion selection
        /// </summary>
        /// <param name="search">current text</param>
        /// <param name="item">selected suggestion</param>
        /// <returns></returns>
        private string QuerySelector(string search, string item)
        {
            // Get word
            string? word = search.Split(' ').LastOrDefault();
            
            // If none, just add it to the end
            if (string.IsNullOrEmpty(word))
            {
                return search + item;
            }
            
            // Otherwise, remove the last word (suggestion) and emplace
            return search.Remove(search.Length - word.Length) + item;
        }

        /// <summary>
        /// All suggestions
        /// </summary>
        private string[] AutoSource = new[] { "ip:", "app:", "port:", "pid:", "api:" };
        
        /// <summary>
        /// View model helper
        /// </summary>
        private ViewModels.ConnectViewModel? VM => DataContext as ViewModels.ConnectViewModel;
    }
}