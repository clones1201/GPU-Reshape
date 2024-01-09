﻿// 
// The MIT License (MIT)
// 
// Copyright (c) 2024 Advanced Micro Devices, Inc.,
// Fatalist Development AB (Avalanche Studio Group),
// and Miguel Petersen.
// 
// All Rights Reserved.
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
using System.Reactive;
using System.Reactive.Subjects;
using Bridge.CLR;
using Message.CLR;
using ReactiveUI;
using Studio.Models.Workspace;
using Studio.ViewModels.Workspace.Adapter;

namespace Studio.ViewModels.Workspace
{
    public class VirtualConnectionViewModel : ReactiveObject, IVirtualConnectionViewModel
    {
        /// <summary>
        /// Invoked during connection
        /// </summary>
        public ISubject<Unit> Connected { get; }

        /// <summary>
        /// Invoked during connection rejection
        /// </summary>
        public ISubject<Unit> Refused { get; }

        /// <summary>
        /// Local time on the remote endpoint
        /// </summary>
        public DateTime LocalTime
        {
            get => _localTime;
            set => this.RaiseAndSetIfChanged(ref _localTime, value);
        }

        /// <summary>
        /// Endpoint application info
        /// </summary>
        public ApplicationInfoViewModel? Application { get; set; }

        /// <summary>
        /// Underlying bridge of this connection
        /// </summary>
        public IBridge? Bridge => null;

        /// <summary>
        /// Constructor
        /// </summary>
        public VirtualConnectionViewModel()
        {
            Connected = new Subject<Unit>();
            Refused = new Subject<Unit>();
        }

        /// <summary>
        /// Get the shared bus
        /// </summary>
        /// <returns></returns>
        public OrderedMessageView<ReadWriteMessageStream> GetSharedBus()
        {
            return _sharedStream;
        }

        /// <summary>
        /// Commit the bus
        /// </summary>
        public void Commit()
        {
            _sharedStream = new(new ReadWriteMessageStream());
        }
                
        /// <summary>
        /// Dummy stream
        /// </summary>
        private OrderedMessageView<ReadWriteMessageStream> _sharedStream = new(new ReadWriteMessageStream());

        /// <summary>
        /// Internal local time
        /// </summary>
        private DateTime _localTime;
    }
}