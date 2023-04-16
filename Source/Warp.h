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
#include "Transport.h"

class Warp : public IAudioProcessor, public IDrawableModule, public ITextEntryListener, public IAudioPoller
{
public:
   Warp();
   static IDrawableModule* Create() { return new Warp(); }
   static bool AcceptsAudio() { return true; }

   void Init() override;
   void CreateUIControls() override;
   void Render() override;
   bool HasDebugDraw() const override { return true; }

   //IAudioSource
   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   //IAudioPoller
   void OnTransportAdvanced(float amount) override;

   //ITextEntryListener
   void TextEntryComplete(TextEntry* entry) override;

   virtual void LoadLayout(const ofxJSONElement& moduleInfo) override;
   virtual void SetUpFromSaveData() override;

   bool IsEnabled() const override { return mEnabled; }

private:
   //IDrawableModule
   void DrawModule() override;
   void DrawModuleUnclipped() override;
   void GetModuleDimensions(float& w, float& h) override
   {
      w = 118;
      h = 22;
   }
   void Exit() override;

   static std::map<std::string, std::vector<Warp*>> mInputs;
   static std::map<std::string, std::vector<Warp*>> mOutputs;
   static std::map<std::string, Warp*> mLastOutput;

   static std::vector<Warp*>* GetInputsByIdent(std::string ident);
   static std::vector<Warp*>* GetOutputsByIdent(std::string ident);

   static void AddInputByIdent(Warp* warp, std::string ident);
   static void AddOutputByIdent(Warp* warp, std::string ident);
   static void RemoveInputByIdent(Warp* warp, std::string ident);
   static void RemoveOutputByIdent(Warp* warp, std::string ident);

   enum class Behavior
   {
      None,
      Input,
      Output
   };
   Behavior mBehavior;

   std::string mIdent{ "default" };
   std::string mIdentPrev{ "default" };
   TextEntry* mIdentEntry;
};
