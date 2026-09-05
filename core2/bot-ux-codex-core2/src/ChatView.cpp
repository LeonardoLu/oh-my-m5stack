// ChatView implementation. See ChatView.h for the contract.
#include "ChatView.h"

#include <Arduino.h>  // millis()
#include <BotUx.h>    // BotUx::Mood
#include <stdio.h>    // snprintf
#include <string.h>   // strlen/strstr/memcpy

namespace {
// transcript strip: right of the 72px bot tile, y 24..96
const int16_t kX = 72;
const int16_t kY = 24;
const int16_t kW = 248;
const int16_t kH = 72;
} // namespace

void ChatView::begin(M5Canvas* cv, botux::BotUx* bot, const botux::BotUx::Style* style,
                     const char* clockBuf, const char* statusBuf)
{
    _cv = cv;
    _bot = bot;
    _style = style;
    _clock = clockBuf;
    _status = statusBuf;
}

void ChatView::_append(const char* text, bool prompt)
{
    Msg& m = _msgs[(_head + _count) % kMaxLines];
    int n = 0;
    while (text[n] && n < kLineLen - 1) { m.text[n] = text[n]; n++; }
    m.text[n] = 0;
    m.prompt = prompt;
    if (_count < kMaxLines) _count++;
    else _head = (_head + 1) % kMaxLines; // drop oldest
}

void ChatView::_buildReply(const char* prompt)
{
    // Canned responses, matched by substring (order matters: "time" first).
    if (strstr(prompt, "time"))
        snprintf(_replyBuf, sizeof(_replyBuf), "it's %s", _clock ? _clock : "??:??");
    else if (strstr(prompt, "joke"))
        snprintf(_replyBuf, sizeof(_replyBuf), "why did the dev cross the road? to git to the other side");
    else if (strstr(prompt, "hello") || strstr(prompt, "hi"))
        snprintf(_replyBuf, sizeof(_replyBuf), "hey! what can I do for you?");
    else if (strstr(prompt, "status") || strstr(prompt, "battery"))
        snprintf(_replyBuf, sizeof(_replyBuf), "%s", _status ? _status : "unknown");
    else
        snprintf(_replyBuf, sizeof(_replyBuf), "interesting - tell me more");

    _replyLen = (int)strlen(_replyBuf);
    _replyShown = 0;
}

void ChatView::setListening()
{
    if (_state == Thinking || _state == Speaking) return; // don't interrupt a reply
    _state = Listening;
    _bot->setMood(botux::BotUx::Mood::Listening);
    _bot->setTalking(false);
}

void ChatView::submit(const char* prompt)
{
    _append(prompt, true);
    _buildReply(prompt);
    _state = Thinking;
    // deterministic-ish think window 800..1200 ms
    uint32_t delay = 800u + ((uint32_t)strlen(prompt) * 31u % 400u);
    _stateUntil = millis() + delay;
    _bot->setMood(botux::BotUx::Mood::Thinking);
    _bot->setTalking(false);
}

void ChatView::_startSpeaking(uint32_t now)
{
    _state = Speaking;
    _speakStart = now;
    _bot->setMood(botux::BotUx::Mood::Speaking);
    _bot->setTalking(true);
}

void ChatView::update(uint32_t now)
{
    switch (_state)
    {
        case Thinking:
            if (now >= _stateUntil) _startSpeaking(now);
            break;

        case Speaking:
        {
            uint32_t el = now - _speakStart;
            int target = (int)(el / 40);      // ~40 ms/char typewriter
            if (el > 2000) target = _replyLen; // 2 s cap → reveal all
            if (target > _replyLen) target = _replyLen;
            _replyShown = target;
            if (_replyShown >= _replyLen)
            {
                _append(_replyBuf, false);
                _replyLen = 0;
                _bot->setTalking(false);
                _bot->setMood(botux::BotUx::Mood::Idle);
                _state = Settling;
                _stateUntil = now + 600;
            }
            break;
        }

        case Settling:
            if (now >= _stateUntil) _state = Idle;
            break;

        default:
            break;
    }
}

void ChatView::applyMood()
{
    switch (_state)
    {
        case Listening: _bot->setMood(botux::BotUx::Mood::Listening); _bot->setTalking(false); break;
        case Thinking:  _bot->setMood(botux::BotUx::Mood::Thinking);  _bot->setTalking(false); break;
        case Speaking:  _bot->setMood(botux::BotUx::Mood::Speaking);  _bot->setTalking(true);  break;
        default:        _bot->setMood(botux::BotUx::Mood::Idle);      _bot->setTalking(false); break;
    }
}

void ChatView::draw()
{
    _cv->fillRect(kX, kY, kW, kH, _style->bgColor);
    const int16_t lineH = 12;
    int maxVis = kH / lineH;

    bool hasTransient = (_state == Thinking || _state == Speaking);
    int total = _count + (hasTransient ? 1 : 0);
    int skip = (total > maxVis) ? (total - maxVis) : 0;

    _cv->setTextDatum(top_left);
    _cv->setTextSize(1.0f);

    for (int e = skip; e < total; e++)
    {
        int16_t ypos = kY + (e - skip) * lineH;
        if (e < _count)
        {
            const Msg& m = _msgs[(_head + e) % kMaxLines];
            char buf[kLineLen + 2];
            if (m.prompt) snprintf(buf, sizeof(buf), "> %s", m.text);
            else          snprintf(buf, sizeof(buf), "  %s", m.text);
            _cv->setTextColor(m.prompt ? _style->accentColor : _style->eyeColor);
            _cv->drawString(buf, kX + 4, ypos);
        }
        else if (_state == Thinking)
        {
            _cv->setTextColor(_style->accentColor);
            _cv->drawString("...", kX + 4, ypos);
        }
        else // Speaking — partial reply + cursor
        {
            char buf[81];
            int n = (_replyShown > 80) ? 80 : _replyShown;
            memcpy(buf, _replyBuf, (size_t)n);
            buf[n] = '|';
            buf[n + 1] = 0;
            _cv->setTextColor(_style->eyeColor);
            _cv->drawString(buf, kX + 4, ypos);
        }
    }
}
