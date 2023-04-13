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

#pragma once
#include "IAudioEffect.h"
#include "Slider.h"
#include "Checkbox.h"
#include "IAudioProcessor.h"
#include "PatchCableSource.h"

class Warp : public IAudioProcessor, public IDrawableModule, public ITextEntryListener
{
public:
   Warp();
   static IDrawableModule* Create() { return new Warp(); }
   static bool AcceptsAudio() { return true; }

   void CreateUIControls() override;

   //IAudioSource
   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   //ITextEntryListener
   void TextEntryComplete(TextEntry* entry) override;

   virtual void LoadLayout(const ofxJSONElement& moduleInfo) override;
   virtual void SetUpFromSaveData() override;

   bool IsEnabled() const override { return mEnabled; }

   static std::map<std::string, ChannelBuffer*> mBuffers;
   ChannelBuffer* GetBufferByIdent(std::string ident);

private:
   //IDrawableModule
   void DrawModule() override;
   void GetModuleDimensions(float& w, float& h) override
   {
      w = 118;
      h = 22;
   }

   std::string mIdent {"default"};
   std::string mIdentPrev {"default"};
   TextEntry* mIdentEntry;

};
