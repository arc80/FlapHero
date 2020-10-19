#pragma once
#include <flapGame/Core.h>

namespace flap {

struct Button {
    enum Result {
        NotHandled,
        Handled,
        Clicked,
    };

    struct State {
        // ply make switch
        struct Up {
        };
        struct Down {
        };
        struct Released {
            float time = 0.f;
        };
#include "codegen/switch-flap-Button-State.inl" //@@ply
    };
    State state;
    bool wasClicked = false;
    float timeSinceClicked = 0;

    float getScale();
    Result doInput(bool down, bool isInside);
    bool update(float dt);
};

} // namespace flap
