#ifndef DICE_TEMPLATELIBRARY_FIXEDCHANNEL_HPP
#define DICE_TEMPLATELIBRARY_FIXEDCHANNEL_HPP

#include <dice/template-library/channel.hpp>

namespace dice::template_library {

    /**
     * A multi producer, multi consumer channel/queue with a fixed capacity
     * @warning `close()` must be called once the producing threads are done, otherwise the reading thread will hang indefinitely
     *
     * Memory for all elements is allocated in the constructor, pushing and popping do not allocate.
     * Pushing is noexcept if constructing the element is noexcept.
     *
     * @tparam T value type of the channel
     */
    template<typename T>
    struct fixed_channel : channel_detail::basic_channel<T, true> {
        using channel_detail::basic_channel<T, true>::basic_channel;
    };

}  // namespace dice::template_library

#endif  // DICE_TEMPLATELIBRARY_FIXEDCHANNEL_HPP
