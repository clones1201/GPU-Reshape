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
using System.Linq;
using System.Reactive;
using System.Reactive.Disposables;
using System.Reactive.Linq;
using System.Reactive.Subjects;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using AvaloniaEdit.TextMate;
using AvaloniaEdit.Utils;
using DynamicData;
using DynamicData.Binding;
using ReactiveUI;
using Runtime.ViewModels.Shader;
using Studio.Extensions;
using Studio.Models.Workspace.Objects;
using Studio.ViewModels.Shader;
using Studio.ViewModels.Workspace.Objects;
using Studio.Views.Editor;
using TextMateSharp.Grammars;

namespace Studio.Views.Shader
{
    public partial class CodeShaderContentView : UserControl, IViewFor
    {
        /// <summary>
        /// Assigned view model
        /// </summary>
        public object? ViewModel
        {
            get => DataContext;
            set => DataContext = value;
        }

        public CodeShaderContentView()
        {
            InitializeComponent();

            // Create the registry
            var registryOptions = new RegistryOptions(ThemeName.DarkPlus);

            // Create a text mate instance
            var textMate = Editor.InstallTextMate(registryOptions);

            // Set the default grammar (just assume .hlsl)
            textMate.SetGrammar(registryOptions.GetScopeByLanguageId(registryOptions.GetLanguageByExtension(".hlsl").Id));

            // Create background renderer
            _validationBackgroundRenderer = new ValidationBackgroundRenderer
            {
                Document = Editor.Document
            };

            // Create marker service
            _validationTextMarkerService = new ValidationTextMarkerService
            {
                Document = Editor.Document
            };

            // Add renderers
            Editor.TextArea.TextView.BackgroundRenderers.Add(_validationBackgroundRenderer);
            Editor.TextArea.TextView.BackgroundRenderers.Add(_validationTextMarkerService);
            Editor.TextArea.TextView.LineTransformers.Add(_validationTextMarkerService);

            // Bind pointer
            Editor.TextArea.TextView.Events().PointerMoved.Subscribe(x => OnTextPointerMoved(x));
            Editor.TextArea.TextView.Events().PointerPressed.Subscribe(x => OnTextPointerPressed(x));

            // Bind redraw
            Editor.TextArea.TextView.VisualLinesChanged += OnTextVisualLinesChanged;
            
            // Add services
            IServiceContainer services = Editor.Document.GetService<IServiceContainer>();
            services?.AddService(typeof(ValidationTextMarkerService), _validationTextMarkerService);

            // Common styling
            Editor.Options.IndentationSize = 4;

            // Bind contents
            this.WhenAnyValue(x => x.DataContext)
                .WhereNotNull()
                .Cast<CodeShaderContentViewModel>()
                .WhereNotNull()
                .Subscribe(codeViewModel =>
                {
                    // Update services
                    _validationTextMarkerService.ShaderContentViewModel = codeViewModel;
                    _validationBackgroundRenderer.ShaderContentViewModel = codeViewModel;

                    // Bind object model
                    codeViewModel.WhenAnyValue(y => y.Object).WhereNotNull().Subscribe(_object =>
                    {
                        // Set objects
                        _validationBackgroundRenderer.ValidationObjects = _object.ValidationObjects;

                        // Bind objects
                        _object.ValidationObjects.ToObservableChangeSet()
                            .AsObservableList()
                            .Connect()
                            .OnItemAdded(OnValidationObjectAdded)
                            .OnItemRemoved(OnValidationObjectAdded)
                            .Subscribe();

                        // Raise navigation changes on file additions
                        _object.FileViewModels.ToObservableChangeSet()
                            .AsObservableList()
                            .Connect()
                            .OnItemAdded(x =>
                            {
                                if (codeViewModel.NavigationLocation != null)
                                {
                                    UpdateNavigationLocation(codeViewModel, codeViewModel.NavigationLocation.Value);
                                }
                            })
                            .Subscribe();
                        
                        // Bind status
                        _object.WhenAnyValue(o => o.AsyncStatus).Subscribe(status =>
                        {
                            if (status.HasFlag(AsyncShaderStatus.NotFound))
                            {
                                Editor.Text = Studio.Resources.Resources.Shader_NotFound;
                            }
                            else if (status.HasFlag(AsyncShaderStatus.NoDebugSymbols))
                            {
                                Editor.Text = Studio.Resources.Resources.Shader_NoDebugSymbols;
                            }
                        });
                    });

                    // Bind selected contents
                    codeViewModel.WhenAnyValue(y => y.SelectedShaderFileViewModel, y => y?.Contents)
                        .WhereNotNull()
                        .Subscribe(contents =>
                        {
                            // Set offset to start
                            Editor.TextArea.Caret.Line = 0;
                            Editor.TextArea.Caret.Column = 0;
                            Editor.TextArea.Caret.BringCaretToView();
                            
                            // Clear and set, avoids internal replacement reformatting hell
                            Editor.Text = string.Empty;
                            Editor.Text = contents;

                            _validationTextMarkerService.ResumarizeValidationObjects();
                        });

                    // Bind navigation location
                    codeViewModel.WhenAnyValue(y => y.NavigationLocation)
                        .WhereNotNull()
                        .Subscribe(location => UpdateNavigationLocation(codeViewModel, location!.Value));
                });

            // Bind object change
            _validationObjectChanged
                .Window(() => Observable.Timer(TimeSpan.FromMilliseconds(250)))
                .ObserveOn(RxApp.MainThreadScheduler)
                .Subscribe(_ => OnValidationObjectChanged());
        }

        /// <summary>
        /// On redraw
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void OnTextVisualLinesChanged(object? sender, EventArgs e)
        {
            // Valid context?
            if (DetailViewHost.DataContext == null || _validationTextMarkerService.SelectedObject == null)
            {
                return;
            }

            // Get the visual line of the selected object
            if (Editor.TextArea.TextView.GetVisualLine(_validationTextMarkerService.SelectedObject.Segment?.Location.Line + 1 ?? 0) is not {} line)
            {
                return;
            }

            // Determine effective offset
            double top    = Editor.TextArea.TextView.VerticalOffset;
            double offset = line.VisualTop - top + Editor.TextArea.TextView.DefaultLineHeight;
            
            // Scroll detail view to offset
            DetailViewHost.Margin = new Thickness(0, offset, 0, -offset);
        }

        /// <summary>
        /// On pointer moved
        /// </summary>
        /// <param name="e"></param>
        private void OnTextPointerMoved(PointerEventArgs e)
        {
            ValidationObject? validationObject = _validationTextMarkerService.FindObjectUnder(Editor.TextArea.TextView, e.GetPosition(Editor.TextArea.TextView));
            
            // Ignore if same
            if (validationObject == _validationTextMarkerService.HighlightObject)
            {
                return;
            }

            // Set highlight and redraw
            _validationTextMarkerService.HighlightObject = validationObject;
            Editor.TextArea.TextView.Redraw();
        }

        /// <summary>
        /// On pointer pressed
        /// </summary>
        /// <param name="e"></param>
        private void OnTextPointerPressed(PointerPressedEventArgs e)
        {
            ValidationObject? validationObject = _validationTextMarkerService.FindObjectUnder(Editor.TextArea.TextView, e.GetPosition(Editor.TextArea.TextView));
            if (validationObject == null)
            {
                _validationTextMarkerService.SelectedObject = null;
                DetailViewHost.ViewHost.ViewModel = null;
                return;
            }

            // Always handled at this point
            e.Handled = true;

            // Changed?
            if (validationObject == _validationTextMarkerService.SelectedObject)
            {
                return;
            }
            
            // Bind detail context
            validationObject?.WhenAnyValue(x => x.DetailViewModel).Subscribe(x =>
            {
                var vm = DataContext as CodeShaderContentViewModel;
                
                DetailViewHost.ViewHost.ViewModel = x ?? new MissingDetailViewModel()
                {
                    Object = vm?.Object,
                    PropertyCollection = vm?.PropertyCollection
                };
            }).DisposeWithClear(_detailDisposable);

            // Update selected and redraw
            _validationTextMarkerService.SelectedObject = validationObject;
            Editor.TextArea.TextView.Redraw();
        }

        private void UpdateNavigationLocation(CodeShaderContentViewModel codeViewModel, ShaderLocation location)
        {
            // Attempt to find file vm
            ShaderFileViewModel? fileViewModel = codeViewModel.Object?.FileViewModels.FirstOrDefault(x => x.UID == location.FileUID);
            if (fileViewModel == null)
            {
                return;
            }

            // Update selected file
            codeViewModel.SelectedShaderFileViewModel = fileViewModel;
                            
            // Scroll to target
            Editor.TextArea.Caret.Line = location.Line;
            Editor.TextArea.Caret.Column = location.Column;
            Editor.TextArea.Caret.BringCaretToView();
        }
        
        /// <summary>
        /// Invoked on object changes
        /// </summary>
        private void OnValidationObjectChanged()
        {
            Editor.TextArea.TextView.InvalidateVisual();
        }

        /// <summary>
        /// Invoked on object added
        /// </summary>
        /// <param name="validationObject"></param>
        private void OnValidationObjectAdded(ValidationObject validationObject)
        {
            // Bind count and contents
            validationObject
                .WhenAnyValue(x => x.Count, x => x.Content)
                .Subscribe(x => _validationObjectChanged.OnNext(new Unit()));

            _validationTextMarkerService.Add(validationObject);
        }

        /// <summary>
        /// Invoked on object removed
        /// </summary>
        /// <param name="validationObject"></param>
        private void OnValidationObjectRemoved(ValidationObject validationObject)
        {
            _validationTextMarkerService.Remove(validationObject);
        }

        /// <summary>
        /// Background rendering
        /// </summary>
        private ValidationBackgroundRenderer _validationBackgroundRenderer;

        /// <summary>
        /// Text marker service, hosts transformed objects
        /// </summary>
        private ValidationTextMarkerService _validationTextMarkerService;

        /// <summary>
        /// Internal proxy observable
        /// </summary>
        private ISubject<Unit> _validationObjectChanged = new Subject<Unit>();

        /// <summary>
        /// Disposable for detailed data
        /// </summary>
        private CompositeDisposable _detailDisposable = new();
    }
}