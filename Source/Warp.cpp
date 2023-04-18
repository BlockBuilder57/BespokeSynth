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
void Warp::Exit()
{
   IDrawableModule::Exit();
   RemoveInputByIdent(this, mIdent);
   RemoveOutputByIdent(this, mIdent);
}

std::map<std::string, std::vector<Warp*>> Warp::mInputs{};
std::map<std::string, std::vector<Warp*>> Warp::mOutputs{};
std::map<std::string, Warp*> Warp::mLastOutput;

std::vector<Warp*>* Warp::GetInputsByIdent(std::string ident)
{
   if (ident.empty())
      return nullptr;

   // create the vector for this ident if it doesn't already exist
   if (mInputs.find(ident) == mInputs.end())
      mInputs[ident] = {};

   return &mInputs[ident];
}
std::vector<Warp*>* Warp::GetOutputsByIdent(std::string ident)
{
   if (ident.empty())
      return nullptr;

   // create the vector for this ident if it doesn't already exist
   if (mOutputs.find(ident) == mOutputs.end())
      mOutputs[ident] = {};

   return &mOutputs[ident];
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
      //ofLog() << warp->Name() << " becoming an input for " << ident << "!";
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
      //ofLog() << warp->Name() << " becoming an output for " << ident << "!";
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
      //ofLog() << warp->Name() << " uninputting for " << ident;
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
      //ofLog() << warp->Name() << " unoutputting for " << ident;
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

void Warp::Render()
{
   IDrawableModule::Render();

   if (!mEnabled)
      return;

   if (mBehavior != Warp::Behavior::Output)
      return;

   std::vector<Warp*>* inputs = GetInputsByIdent(mIdent);
   if (inputs == nullptr || inputs->empty())
      return;

   ofRectangle rThis = GetRect();
   float tbThis = HasTitleBar() ? TitleBarHeight() : 0;

   const float lineWidth = 0.75f;
   const float lineScale = 6.f;
   const float lineAlpha = 100.0f;
   const float markerScaleStart = lineScale * 1.5f;
   const float markerScaleEnd = lineScale * 0.5f;
   const float markerDist = lineScale * 0.5f;
   const ofColor lineColor = ofColor::yellow;

   ofPushMatrix();
   ofPushStyle();

   ofSetLineWidth(lineWidth);

   for (Warp* input : *inputs)
   {
      // don't use disabled inputs
      if (!input->IsEnabled())
         continue;

      RollingBuffer* vizBuff = input->GetVizBuffer();
      assert(vizBuff);
      int numSamples = vizBuff->Size();

      bool allZero = true;
      for (int ch = 0; ch < vizBuff->NumChannels(); ++ch)
      {
         for (int i = 0; i < numSamples && allZero; ++i)
         {
            if (vizBuff->GetSample(i, ch) != 0)
               allZero = false;
         }
      }

      if (allZero)
         continue;

      ofRectangle rInput = input->GetRect();
      float tbInput = HasTitleBar() ? TitleBarHeight() : 0;

      ofVec2f start, end;

      FindClosestSides(rInput.x, rInput.y - tbInput, rInput.width, rInput.height + tbInput,
                       rThis.x, rThis.y - tbThis, rThis.width, rThis.height + tbThis,
                       start.x, start.y, end.x, end.y);

      float wireLength = sqrtf((end - start).lengthSquared());
      float wireSection = std::min(wireLength, 175.f);

      ofVec2f delta = (end - start) / wireLength;
      ofVec2f deltaParallel(delta.y, -delta.x);
      float cableStepSize = 6.f;

      for (int ch = 0; ch < vizBuff->NumChannels(); ++ch)
      {
         ofColor drawColor;
         if (ch == 0)
            drawColor.set(lineColor.r, lineColor.g, lineColor.b, lineColor.a);
         else
            drawColor.set(lineColor.b, lineColor.r, lineColor.g, lineColor.a);

         ofVec2f offset((ch - (vizBuff->NumChannels() - 1) * .5f) * 2 * delta.y, (ch - (vizBuff->NumChannels() - 1) * .5f) * 2 * -delta.x);


         bool skipGap = true;
         float actualSection = wireSection;
         if (wireLength - markerDist * 2 > wireSection)
         {
            // draw the gap if the wire extends too far
            skipGap = false;
            actualSection *= 0.5f;
         }

         // locations for markers and end/start points for the sections
         ofVec2f sec1Mark = (start + offset) + (delta*actualSection);
         ofVec2f sec2Mark = (start + offset) + (delta*(wireLength - actualSection));

         ofBeginShape();
         ofVertex(start + offset);
         for (float i = 1; i < actualSection - 1; i += cableStepSize)
         {
            auto pos = (start + offset) + (delta*i);
            float sample = vizBuff->GetSample((i / wireLength * numSamples), ch);
            if (isnan(sample))
            {
               drawColor = ofColor(255, 0, 0);
               sample = 0;
            }
            else
            {
               sample = sqrtf(fabsf(sample)) * (sample < 0 ? -1 : 1);
               sample = ofClamp(sample, -1.0f, 1.0f);
               drawColor.a = fabsf(sample) * lineAlpha;
            }

            pos.x += lineScale * sample * -delta.y;
            pos.y += lineScale * sample * delta.x;
            ofVertex(pos);
         }
         ofVertex(sec1Mark);
         ofSetColor(drawColor);
         ofEndShape();

         if (skipGap)
            continue;

         // markers look like a voltage source on wiring diagrams: wwww> | >

         // first marker
         ofBeginShape();
         ofVertex(sec1Mark + (delta * markerDist) + (deltaParallel * markerScaleStart));
         ofVertex(sec1Mark + (delta * markerDist) - (deltaParallel * markerScaleStart));
         ofEndShape();

         ofBeginShape();
         ofVertex(sec1Mark + (delta * markerDist * 2) + (deltaParallel * markerScaleEnd));
         ofVertex(sec1Mark + (delta * markerDist * 3));
         ofVertex(sec1Mark + (delta * markerDist * 2) - (deltaParallel * markerScaleEnd));
         ofEndShape();

         // second section
         ofBeginShape();
         ofVertex(sec2Mark);
         for (float i = wireLength - actualSection; i < wireLength - 1; i += cableStepSize)
         {
            auto pos = (start + offset) + (delta*i);
            float sample = vizBuff->GetSample((i / wireLength * numSamples), ch);
            if (isnan(sample))
            {
               drawColor = ofColor(255, 0, 0);
               sample = 0;
            }
            else
            {
               sample = sqrtf(fabsf(sample)) * (sample < 0 ? -1 : 1);
               sample = ofClamp(sample, -1.0f, 1.0f);
               drawColor.a = fabsf(sample) * lineAlpha;
            }

            pos.x += lineScale * sample * -delta.y;
            pos.y += lineScale * sample * delta.x;
            ofVertex(pos);
         }
         ofVertex(end + offset);
         ofSetColor(drawColor);
         ofEndShape();

         // second marker
         ofBeginShape();
         ofVertex(sec2Mark - (delta * markerDist) + (deltaParallel * markerScaleStart));
         ofVertex(sec2Mark - (delta * markerDist) - (deltaParallel * markerScaleStart));
         ofEndShape();

         ofBeginShape();
         ofVertex(sec2Mark - (delta * markerDist * 3) + (deltaParallel * markerScaleEnd));
         ofVertex(sec2Mark - (delta * markerDist * 2));
         ofVertex(sec2Mark - (delta * markerDist * 3) - (deltaParallel * markerScaleEnd));
         ofEndShape();
      }
   }

   ofPopStyle();
   ofPopMatrix();
}

void Warp::Process(double time)
{
   PROFILER(Warp);

   if (!mEnabled)
      return;

   SyncBuffers();

   if (mIdentPrev != mIdent) {
      // when switching away from an ident, we need to update the maps
      // this can be done in a few places, but most notably the text box
      // OSC controls and snapshots modify the ident without going through TextEntryComplete,
      // which is why we can't just rely on that
      RemoveInputByIdent(this, mIdentPrev);
      RemoveOutputByIdent(this, mIdentPrev);

      mIdentPrev = mIdent;
   }

   // if we have nothing pointing to us, then we're assuredly not an input
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
      // no function, ensure we're not in the maps
      RemoveInputByIdent(this, mIdent);
      RemoveOutputByIdent(this, mIdent);
   }
   else if (GetTarget() == nullptr)
      AddInputByIdent(this, mIdent);
   else
      AddOutputByIdent(this, mIdent);

   auto cableSrc = IDrawableModule::GetPatchCableSource();

   if (cableSrc != nullptr && !cableSrc->Enabled())
      cableSrc->SetEnabled(true);

   if (mBehavior == Warp::Behavior::Input)
   {
      // pulse the viz buffer - we're wireless, so we want to communicate to the user as much as possible
      for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
         GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), GetBuffer()->BufferSize(), ch);

      if (cableSrc != nullptr)
         cableSrc->SetEnabled(false);
   }

   if (mBehavior == Warp::Behavior::Output)
   {
      mLastOutput[mIdent] = this;
   }
   else
   {
      // we'll need to clear our own buffer if nothing will for us
      bool shouldClear = true;

      std::vector<Warp*>* outputs = GetOutputsByIdent(mIdent);
      if (outputs != nullptr)
      {
         for (Warp* output : *outputs) {
            if (output->IsEnabled()) {
               shouldClear = false;
               break;
            }
         }
      }

      if (shouldClear)
         GetBuffer()->Clear();
   }
}
void Warp::OnTransportAdvanced(float amount)
{
   if (mBehavior != Warp::Behavior::Output)
      return;

   if (!mEnabled)
      return;

   if (GetTarget() == nullptr)
      return;

   GetBuffer()->SetNumActiveChannels(1);

   std::vector<Warp*>* inputs = GetInputsByIdent(mIdent);
   std::vector<Warp*>* outputs = GetOutputsByIdent(mIdent);
   if (inputs == nullptr || outputs == nullptr)
      return;

   if (!inputs->empty())
   {
      // we check inputs with the name of our ident, and copy their buffer in
      for (Warp* warp : *inputs)
      {
         // don't use disabled inputs
         if (!warp->IsEnabled())
            continue;

         ChannelBuffer* warpBuf = warp->GetBuffer();

         if (GetBuffer()->NumActiveChannels() < warpBuf->NumActiveChannels())
            GetBuffer()->SetNumActiveChannels(warpBuf->NumActiveChannels());

         for (int ch = 0; ch < warpBuf->NumActiveChannels(); ++ch)
            Add(GetBuffer()->GetChannel(ch), warpBuf->GetChannel(ch), warpBuf->BufferSize());

         // the last output to be processed is the one to reset the buffers on the inputs
         // "close the door on your way out"
         if (mLastOutput[mIdent] == this)
            warpBuf->Reset();
      }
   }

   SyncOutputBuffer(GetBuffer()->NumActiveChannels());

   ChannelBuffer* out = GetTarget()->GetBuffer();

   for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
   {
      auto channel = GetBuffer()->GetChannel(ch);

      GetVizBuffer()->WriteChunk(channel, GetBuffer()->BufferSize(), ch);
      Add(out->GetChannel(ch), channel, out->BufferSize());
   }

   GetBuffer()->Clear();
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

      DrawTextNormal("~ Local info", 0, yOff += 10);
      //DrawTextNormal("Ident: " + mIdent, 0, yOff += 10);
      //DrawTextNormal("Prev ident: " + mIdentPrev, 0, yOff += 10);
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
            if (mLastOutput[mIdent] == this)
            DrawTextNormal("the last output for " + mIdent, 0, yOff += 10);
            break;
         }
         default:
         {
            DrawTextNormal("No behavior!", 0, yOff += 10);
            break;
         }
      }

      DrawTextNormal("~ Global info", 0, yOff += 20);
      DrawTextNormal("Idents (input): " + std::to_string(mInputs.size()), 0, yOff += 10);
      DrawTextNormal("Idents (output): " + std::to_string(mOutputs.size()), 0, yOff += 10);
      DrawTextNormal("Inputs on this ident: " + std::to_string(mInputs[mIdent].size()), 0, yOff += 10);
      DrawTextNormal("Outputs on this ident: " + std::to_string(mOutputs[mIdent].size()), 0, yOff += 10);
      if (mLastOutput[mIdent] != nullptr)
         DrawTextNormal("Last output on this ident: " + std::string(mLastOutput[mIdent]->Name()), 0, yOff += 10);
   }
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
