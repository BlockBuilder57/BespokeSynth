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

void Warp::Init()
{
   IDrawableModule::Init();

   TheTransport->AddAudioPoller(this);
}

std::map<std::string, std::vector<Warp*>> Warp::mInputs{};
std::map<std::string, std::vector<Warp*>> Warp::mOutputs{};

std::vector<Warp*>* Warp::GetInputsByIdent(std::string ident)
{
   if (ident.empty())
      return nullptr;

   // create the vector for this ident if it doesn't already exist
   if (mInputs.find(mIdent) == mInputs.end())
      mInputs[mIdent] = {};

   return &mInputs[mIdent];
}
std::vector<Warp*>* Warp::GetOutputsByIdent(std::string ident)
{
   if (ident.empty())
      return nullptr;

   // create the vector for this ident if it doesn't already exist
   if (mOutputs.find(mIdent) == mOutputs.end())
      mOutputs[mIdent] = {};

   return &mOutputs[mIdent];
}

void Warp::AddInputByIdent(Warp* warp, std::string ident)
{
   // if this is an output, remove it
   RemoveOutputByIdent(warp, ident);

   std::vector<Warp*>* inputs = GetInputsByIdent(ident);
   if (inputs == nullptr)
      return;

   if (std::find(inputs->begin(), inputs->end(), warp) == inputs->end())
   {
      //ofLog() << warp->Name() << " becoming an input for " << mIdent << "!";
      warp->mBehavior = Warp::Behavior::Input;
      inputs->emplace_back(warp);
   }
}
void Warp::AddOutputByIdent(Warp* warp, std::string ident)
{
   // if this is an input, remove it
   RemoveInputByIdent(warp, ident);

   std::vector<Warp*>* outputs = GetOutputsByIdent(ident);
   if (outputs == nullptr)
      return;

   if (std::find(outputs->begin(), outputs->end(), warp) == outputs->end())
   {
      //ofLog() << warp->Name() << " becoming an output for " << mIdent << "!";
      warp->mBehavior = Warp::Behavior::Output;
      outputs->emplace_back(warp);
   }
}
void Warp::RemoveInputByIdent(Warp* warp, std::string ident)
{
   std::vector<Warp*>* inputs = GetInputsByIdent(ident);
   if (inputs == nullptr)
      return;

   if (std::find(inputs->begin(), inputs->end(), warp) != inputs->end())
   {
      //ofLog() << Name() << " uninputting for " << ident;
      warp->mBehavior = Warp::Behavior::None;
      inputs->erase(std::remove(inputs->begin(), inputs->end(), warp), inputs->end());

      if (inputs->empty())
         mInputs.erase(ident);
   }
}
void Warp::RemoveOutputByIdent(Warp* warp, std::string ident)
{
   std::vector<Warp*>* outputs = GetOutputsByIdent(ident);
   if (outputs == nullptr)
      return;

   if (std::find(outputs->begin(), outputs->end(), warp) != outputs->end())
   {
      //ofLog() << Name() << " unoutputting for " << ident;
      warp->mBehavior = Warp::Behavior::None;
      outputs->erase(std::remove(outputs->begin(), outputs->end(), warp), outputs->end());

      if (outputs->empty())
         mOutputs.erase(ident);
   }
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

   SyncBuffers();

   // if we have nothing pointing to us, let's bail
   // FIXME: is there a better way of doing this?

   bool targeted = false;
   std::vector<IDrawableModule*> modules;
   TheSynth->GetAllModules(modules);
   for (IDrawableModule* mod : modules)
   {
      auto cableSrc = mod->GetPatchCableSource();

      if (cableSrc != nullptr && cableSrc->GetTarget() == this)
      {
         targeted = true;
         break;
      }
   }

   if (!targeted && GetTarget() == nullptr)
   {
      RemoveInputByIdent(this, mIdent);
      RemoveOutputByIdent(this, mIdent);
   }
   else if (GetTarget() == nullptr)
      AddInputByIdent(this, mIdent);
   else
      AddOutputByIdent(this, mIdent);

   auto cableSrc = IDrawableModule::GetPatchCableSource();


   if (mBehavior == Warp::Behavior::Input)
   {
      // pulse the viz buffer - we're wireless, so we want to communicate to the user as much as possible
      for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
         GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), GetBuffer()->BufferSize(), ch);

      if (cableSrc != nullptr)
         cableSrc->SetEnabled(false);
   }
   else {
      if (cableSrc != nullptr)
         cableSrc->SetEnabled(true);
   }

   // we'll need to clear our own buffer if nothing will for us
   std::vector<Warp*>* outputs = GetOutputsByIdent(mIdent);
   if (outputs == nullptr || outputs->empty())
      GetBuffer()->Clear();
}

void Warp::OnTransportAdvanced(float amount)
{
   if (mBehavior == Warp::Behavior::Input)
      return;

   if (!mEnabled)
      return;

   std::vector<Warp*>* inputs = GetInputsByIdent(mIdent);
   std::vector<Warp*>* outputs = GetOutputsByIdent(mIdent);
   if (inputs == nullptr || outputs == nullptr)
      return;

   if (!inputs->empty())
   {
      // can happen when we delete the module
      if (GetTarget() == nullptr)
         return;

      ChannelBuffer* out = GetTarget()->GetBuffer();

      // we check inputs with the name of our ident, and copy their buffer in
      for (Warp* warp : *inputs)
      {
         // don't use disabled inputs
         if (!warp->IsEnabled())
            continue;

         //ofLog() << Name() << ": Checking " << warp->Name();
         ChannelBuffer* warpBuf = warp->GetBuffer();

         // unholy matrimony of two buffers, so let's use the lowest channel count to avoid errors
         int channelCount = std::min(out->NumActiveChannels(), warpBuf->NumActiveChannels());
         for (int ch = 0; ch < channelCount; ++ch)
         {
            GetVizBuffer()->WriteChunk(warpBuf->GetChannel(ch), warpBuf->BufferSize(), ch);
            Add(out->GetChannel(ch), warpBuf->GetChannel(ch), warpBuf->BufferSize());
         }

         // the last output to be processed is the one to reset the buffers on the inputs
         // "close the door on your way out"
         if (!outputs->empty() && outputs->front() == this)
         {
            //ofLog() << Name() << ": clearing " << warp->Name() << "'s buffer!";
            warpBuf->Reset();
         }
      }
   }
}

void Warp::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mIdentEntry->Draw();
}

void Warp::DrawModuleUnclipped()
{
   if (!mEnabled)
      return;

   if (mDrawDebug)
   {
      int yOff = 30;
      DrawTextNormal("~ Global info", 0, yOff += 10);
      DrawTextNormal("Idents (input): " + std::to_string(mInputs.size()), 0, yOff += 10);
      DrawTextNormal("Idents (output): " + std::to_string(mOutputs.size()), 0, yOff += 10);
      DrawTextNormal("Inputs on this ident: " + std::to_string(mInputs[mIdent].size()), 0, yOff += 10);
      DrawTextNormal("Outputs on this ident: " + std::to_string(mOutputs[mIdent].size()), 0, yOff += 10);

      DrawTextNormal("~ Local info", 0, yOff += 20);
      switch (mBehavior)
      {
         case Warp::Behavior::Input:
         {
            DrawTextNormal("<IS AN INPUT>", 0, yOff += 10);
            break;
         }
         case Warp::Behavior::Output:
         {
            DrawTextNormal("<IS AN OUTPUT>", 0, yOff += 10);
            if (this == mOutputs[mIdent].front())
               DrawTextNormal("last to output!", 0, yOff += 10);
            break;
         }
         default:
         {
            DrawTextNormal("No behavior!", 0, yOff += 10);
            break;
         }
      }
   }
}

void Warp::Exit()
{
   IDrawableModule::Exit();
   RemoveInputByIdent(this, mIdent);
   RemoveOutputByIdent(this, mIdent);
}

void Warp::TextEntryComplete(TextEntry* entry)
{
   // when switching away from an ident, we need to update the map

   if (mBehavior == Warp::Behavior::Input)
   {
      RemoveInputByIdent(this, mIdentPrev);
      AddInputByIdent(this, mIdent);
   }
   else if (mBehavior == Warp::Behavior::Output)
   {
      RemoveOutputByIdent(this, mIdentPrev);
      AddOutputByIdent(this, mIdent);
   }

   mIdentPrev = mIdent;
}

void Warp::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   mIdent = mModuleSaveData.LoadString("ident", moduleInfo, "default");
   mIdentPrev = mIdent;

   SetUpFromSaveData();
}

void Warp::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
   mIdent = mModuleSaveData.GetString("ident");
   mIdentPrev = mIdent;
}
