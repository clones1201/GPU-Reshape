// 
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
using System.Reactive.Linq;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using DynamicData;
using ReactiveUI;
using Studio.Extensions;
using Studio.ViewModels.Tools;

namespace Studio.Views.Tools
{
    public partial class LogView : UserControl
    {
        public LogView()
        {
            InitializeComponent();

            // Bind context
            this.WhenAnyValue(x => x.DataContext)
                .CastNullable<LogViewModel>()
                .Subscribe(viewModel =>
                {
                    // Bind events to unfiltered collection
                    viewModel.LoggingViewModel?.Events
                        .Connect()
                        .OnItemAdded(x =>
                        {
                            // Check if scroll locked
                            if (viewModel.IsScrollLock && LogListbox.ItemCount > 0)
                            {
                                LogListbox.ScrollIntoView(LogListbox.ItemCount - 1);
                            }
                        })
                        .Subscribe();

                    // Bind scroll lock event
                    viewModel.WhenAnyValue(x => x.IsScrollLock).Subscribe(x =>
                    {
                        if (x && LogListbox.ItemCount > 0)
                        {
                            LogListbox.ScrollIntoView(LogListbox.ItemCount - 1);
                        }
                    });
                });

            // Disable lock on scroll
            LogListbox.Events().PointerWheelChanged.Subscribe(x =>
            {
                if (DataContext is LogViewModel vm)
                {
                    vm.IsScrollLock = false;
                }
            });
        }
    }
}