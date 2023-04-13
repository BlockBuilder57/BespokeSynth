/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2023 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
// Created by block on 4/12/2023.

#include "Warp.h"
#include "SynthGlobals.h"
#include "Profiler.h"
#include "ModularSynth.h"

Warp::Warp()
: IAudioProcessor(gBufferSize)
{
}

std::map<std::string, ChannelBuffer*> Warp::mBuffers{};

ChannelBuffer* Warp::GetBufferByIdent(std::string ident)
{
   if (ident.empty())
      return nullptr;

   // create the buffer of this ident if it doesn't already exist
   if (mBuffers.find(mIdent) == mBuffers.end())
      mBuffers[mIdent] = new ChannelBuffer();

   return mBuffers[mIdent];
}

void Warp::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mIdentEntry = new TextEntry(this, "ident", 5, 2, 12, &mIdent);
}

void Warp::Process(double time)
{
   PROFILER(Warp);

   if (!mEnabled)
      return;

   auto buf = GetBufferByIdent(mIdent);
   if (buf == nullptr)
      return;

   SyncBuffers();

   // FIXME: hack until I can find the proper method of checking for any inputs
   std::string_view name = std::string_view(Name());
   if (name.find("in") != std::string::npos)
   {
      // add the input to the ident buffer, this should support multiple channels just fine
      for (int ch = 0; ch < buf->NumActiveChannels(); ++ch)
      {
         Add(buf->GetChannel(ch), GetBuffer()->GetChannel(ch), buf->BufferSize());
      }
   }

   IAudioReceiver* target = GetTarget();
   if (target)
   {
      ChannelBuffer* out = target->GetBuffer();

      // we'll actually want to match the buffer's channels here, so we're
      // always "broadcasting" what we need to be
      for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
      {
         Add(out->GetChannel(ch), buf->GetChannel(ch), out->BufferSize());
         GetVizBuffer()->WriteChunk(buf->GetChannel(ch), GetBuffer()->BufferSize(), ch);
      }

      // TODO: move resetting somewhere more "global", so multiple modules can output the same ident
      buf->Reset();
   }

   GetBuffer()->Reset();
}

void Warp::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mIdentEntry->Draw();
}

void Warp::TextEntryComplete(TextEntry* entry)
{
   // if we're switching off an ident, we can erase it
   // anything that needs that ident in the future can just make a new one
   if (!mIdentPrev.empty())
   {
      // TODO: i'm bad at memory management with maps, please verify this actually works well
      delete mBuffers[mIdentPrev];
      mBuffers.erase(mIdentPrev);
   }

   mIdentPrev = mIdent;
}

void Warp::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);

   SetUpFromSaveData();
}

void Warp::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
   mIdent = mModuleSaveData.GetString("ident");
}
