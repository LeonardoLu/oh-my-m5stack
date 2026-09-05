// ChatView — transcript + bot mood wiring for the Core2 codex app.
//
// Holds a capped ring buffer of prompt ("…") / reply lines, drives the
// Listening→Thinking→Speaking→Idle conversation state machine, and types the reply
// out with a typewriter effect. The BotUx instance is owned by main; ChatView only
// sets its mood/talking as the conversation advances.
#pragma once

#include <M5GFX.h> // M5Canvas (== LGFX_Sprite)
#include <stdint.h>

#include "BotUx.h" // botux::BotUx + nested Style

class ChatView {
public:
    // clock/status are persistent buffers owned by main (never freed); read live
    // when a canned reply is generated.
    void begin(M5Canvas* cv, botux::BotUx* bot, const botux::BotUx::Style* style,
               const char* clockBuf, const char* statusBuf);

    void setListening();            // prompt focused / a key was pressed
    void submit(const char* prompt); // SEND / enter
    void update(uint32_t nowMs);
    void draw();
    void applyMood();               // re-apply current mood/talk after a gesture override

private:
    enum State : uint8_t { Idle, Listening, Thinking, Speaking, Settling };

    static const int kMaxLines = 12;
    static const int kLineLen = 40;

    struct Msg { char text[kLineLen]; bool prompt; };

    void _append(const char* text, bool prompt);
    void _buildReply(const char* prompt); // fill _replyBuf from canned table
    void _startSpeaking(uint32_t now);

    M5Canvas* _cv = nullptr;
    botux::BotUx* _bot = nullptr;
    const botux::BotUx::Style* _style = nullptr;
    const char* _clock = nullptr;   // "HH:MM" persistent
    const char* _status = nullptr;  // "bat 87% - no signal" persistent

    Msg _msgs[kMaxLines];
    int _count = 0;   // valid entries
    int _head = 0;    // ring start

    State _state = Idle;
    uint32_t _stateUntil = 0;
    uint32_t _speakStart = 0;

    char _replyBuf[80];
    int  _replyLen = 0;
    int  _replyShown = 0;
};
