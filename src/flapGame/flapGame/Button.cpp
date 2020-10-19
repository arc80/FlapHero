#include <flapGame/Core.h>
#include <flapGame/Button.h>
#include <flapGame/DrawContext.h>
#include <flapGame/Assets.h>

namespace flap {

extern SoLoud::Soloud gSoLoud; // SoLoud engine

float Button::getScale() {
    const DrawContext* dc = DrawContext::instance();
    float scale = 1.f;
    if (this->state.down()) {
        scale = 0.85f;
    } else if (auto released = this->state.released()) {
        float t = released->time + dc->fracTime;
        if (t < 0.05f) {
            scale = mix(0.85f, 1.35f, t / 0.08f);
        } else {
            t = clamp(unmix(0.25f, 0.05f, t), 0.f, 1.f);
            scale = mix(1.f, 1.35f, t * t);
        }
    }
    return scale;
}

Button::Result Button::doInput(bool down, bool isInside) {
    if (down) {
        if (isInside) {
            this->state.down().switchTo();
            gSoLoud.play(Assets::instance->buttonDownSound, 1.f);
            return Button::Handled;
        }
    } else {
        if (isInside) {
            this->state.released().switchTo();
            gSoLoud.play(Assets::instance->buttonUpSound, 1.5f);
            this->wasClicked = true;
            return Button::Clicked;
        } else {
            this->state.up().switchTo();
            return Button::Handled;
        }
    }
    return Button::NotHandled;
}

bool Button::update(float dt) {
    if (auto released = this->state.released()) {
        released->time += dt;
        if (released->time >= 0.35f) {
            this->state.up().switchTo();
            return false;
        }
    }
    if (this->wasClicked) {
        this->timeSinceClicked += dt;
    }
    return true;
}

} // namespace flap

#include "codegen/Button.inl" //%%
