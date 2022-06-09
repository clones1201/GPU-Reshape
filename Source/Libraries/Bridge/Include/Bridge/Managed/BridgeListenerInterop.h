#pragma once

#include <Bridge/IBridgeListener.h>
#include <Message/MessageStream.h>

// Managed
#include <vcclr.h>

namespace Bridge::CLR {
    public class BridgeListenerInterop : public ::IBridgeListener {
    public:
        COMPONENT(BridgeListenerInterop);

        /// Constructor
        BridgeListenerInterop(const gcroot<Bridge::CLR::IBridgeListener^>& handle) : handle(handle) {

        }

        /// Overrides
        virtual void Handle(const ::MessageStream* streams, uint32_t count) override {
            // TODO: Batch submission
            for (uint32_t i = 0; i < count; i++) {
                const ::MessageStream& stream = streams[i];

                // Create clr object
                auto clrStream = gcnew Message::CLR::ReadOnlyMessageStream;
                clrStream->ptr = const_cast<uint8_t*>(stream.GetDataBegin());
                clrStream->size = stream.GetByteSize();
                clrStream->count = stream.GetCount();
                clrStream->schema.type = static_cast<Message::CLR::MessageSchemaType>(stream.GetSchema().type);
                clrStream->schema.id = stream.GetSchema().id;
                handle->Handle(clrStream, 1);
            }
        }

        /// Pinned handle
        gcroot<Bridge::CLR::IBridgeListener^> handle;
    };
}
