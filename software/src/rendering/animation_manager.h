#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

// Easing functions
namespace Easing {
  inline float linear(float t) { return t; }
  inline float easeInQuad(float t) { return t * t; }
  inline float easeOutQuad(float t) { return t * (2 - t); }
  inline float easeInOutQuad(float t) { return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t; }
  inline float easeInCubic(float t) { return t * t * t; }
  inline float easeOutCubic(float t) { return (--t) * t * t + 1; }
  inline float easeInOutCubic(float t) { return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1; }
  inline float easeInExpo(float t) { return t == 0 ? 0 : pow(2, 10 * (t - 1)); }
  inline float easeOutExpo(float t) { return t == 1 ? 1 : 1 - pow(2, -10 * t); }
}

// Animation types
enum AnimationType {
  ANIM_SLIDE_LEFT,
  ANIM_SLIDE_RIGHT,
  ANIM_SLIDE_UP,
  ANIM_SLIDE_DOWN,
  ANIM_FADE_IN,
  ANIM_FADE_OUT,
  ANIM_SCALE,
  ANIM_CUSTOM,
};

// Animation state
struct Animation {
  uint32_t id;
  AnimationType type;
  uint32_t startTime;
  uint32_t duration;
  float startValue;
  float endValue;
  float currentValue;
  std::function<float(float)> easingFunc;
  std::function<void(float)> updateFunc;
  std::function<void()> completeFunc;
  bool isComplete;
  bool isPaused;
};

class AnimationManager {
private:
  static const uint32_t FRAME_TIME_US = 16667; // ~60 FPS (1000000/60)
  static const uint32_t MAX_ANIMATIONS = 10;
  
  std::vector<Animation> animations;
  uint32_t lastFrameTime;
  uint32_t nextAnimId;
  bool isAnimating;
  uint32_t frameCount;
  uint32_t lastFPSUpdate;
  float currentFPS;
  
  // Performance metrics
  uint32_t totalFrameTime;
  uint32_t minFrameTime;
  uint32_t maxFrameTime;
  
public:
  AnimationManager() : 
    lastFrameTime(0), 
    nextAnimId(1), 
    isAnimating(false),
    frameCount(0),
    lastFPSUpdate(0),
    currentFPS(0),
    totalFrameTime(0),
    minFrameTime(UINT32_MAX),
    maxFrameTime(0) {
    animations.reserve(MAX_ANIMATIONS);
  }
  
  // Create a new animation
  uint32_t animate(
    AnimationType type,
    float startValue,
    float endValue,
    uint32_t duration,
    std::function<void(float)> updateFunc,
    std::function<void()> completeFunc = nullptr,
    std::function<float(float)> easingFunc = Easing::easeInOutCubic
  ) {
    // Remove completed animations to make room
    cleanupAnimations();
    
    Animation anim;
    anim.id = nextAnimId++;
    anim.type = type;
    anim.startTime = millis();
    anim.duration = duration;
    anim.startValue = startValue;
    anim.endValue = endValue;
    anim.currentValue = startValue;
    anim.easingFunc = easingFunc;
    anim.updateFunc = updateFunc;
    anim.completeFunc = completeFunc;
    anim.isComplete = false;
    anim.isPaused = false;
    
    animations.push_back(anim);
    isAnimating = true;
    
    return anim.id;
  }
  
  // Convenience methods for common animations
  uint32_t slideLeft(int distance, uint32_t duration, 
                     std::function<void(float)> updateFunc,
                     std::function<void()> completeFunc = nullptr) {
    return animate(ANIM_SLIDE_LEFT, 0, distance, duration, updateFunc, completeFunc);
  }
  
  uint32_t slideRight(int distance, uint32_t duration,
                      std::function<void(float)> updateFunc,
                      std::function<void()> completeFunc = nullptr) {
    return animate(ANIM_SLIDE_RIGHT, 0, distance, duration, updateFunc, completeFunc);
  }
  
  uint32_t fadeIn(uint32_t duration,
                  std::function<void(float)> updateFunc,
                  std::function<void()> completeFunc = nullptr) {
    return animate(ANIM_FADE_IN, 0, 1, duration, updateFunc, completeFunc, Easing::easeOutQuad);
  }
  
  uint32_t fadeOut(uint32_t duration,
                   std::function<void(float)> updateFunc,
                   std::function<void()> completeFunc = nullptr) {
    return animate(ANIM_FADE_OUT, 1, 0, duration, updateFunc, completeFunc, Easing::easeInQuad);
  }

  // Update all animations - returns true if frame was rendered
  bool update() {
    uint32_t currentTime = micros();
    
    // Check if enough time has passed for next frame
    if (!isAnimating || (currentTime - lastFrameTime) < FRAME_TIME_US) {
      return false;
    }
    // Track frame timing
    uint32_t frameTime = currentTime - lastFrameTime;
    updatePerformanceMetrics(frameTime);
    lastFrameTime = currentTime;
    
    bool hasActiveAnimations = false;
    uint32_t currentMillis = millis();
    
    // Update all animations
    for (auto& anim : animations) {
      if (anim.isComplete || anim.isPaused) continue;
      
      uint32_t elapsed = currentMillis - anim.startTime;
      
      if (elapsed >= anim.duration) {
      Serial.println("COMPLETE");

        // Animation complete
        anim.currentValue = anim.endValue;
        anim.isComplete = true;
        
        // Final update
        if (anim.updateFunc) {
          anim.updateFunc(anim.currentValue);
        }
        
        // Call completion callback
        if (anim.completeFunc) {
          anim.completeFunc();
        }
      } else {

        // Animation in progress
        float progress = (float)elapsed / anim.duration;
        float easedProgress = anim.easingFunc(progress);
        anim.currentValue = anim.startValue + (anim.endValue - anim.startValue) * easedProgress;
        
        if (anim.updateFunc) {
          anim.updateFunc(anim.currentValue);
        }
        
        hasActiveAnimations = true;
      }
    }
    
    // Stop animating if no active animations
    if (!hasActiveAnimations) {
      isAnimating = false;
    }
    
    // Update FPS counter
    frameCount++;
    if (currentMillis - lastFPSUpdate >= 1000) {
      currentFPS = frameCount;
      frameCount = 0;
      lastFPSUpdate = currentMillis;
    }
    
    return true;
  }
  
  // Control methods
  void pause(uint32_t animId) {
    for (auto& anim : animations) {
      if (anim.id == animId) {
        anim.isPaused = true;
        break;
      }
    }
  }
  
  void resume(uint32_t animId) {
    for (auto& anim : animations) {
      if (anim.id == animId) {
        anim.isPaused = false;
        isAnimating = true;
        break;
      }
    }
  }
  
  void cancel(uint32_t animId) {
    for (auto& anim : animations) {
      if (anim.id == animId) {
        anim.isComplete = true;
        break;
      }
    }
  }
  
  void cancelAll() {
    for (auto& anim : animations) {
      anim.isComplete = true;
    }
    isAnimating = false;
  }
  
  // Status methods
  bool isActive() const { return isAnimating; }
  
  bool isAnimationActive(uint32_t animId) const {
    for (const auto& anim : animations) {
      if (anim.id == animId && !anim.isComplete && !anim.isPaused) {
        return true;
      }
    }
    return false;
  }
  
  float getProgress(uint32_t animId) const {
    for (const auto& anim : animations) {
      if (anim.id == animId) {
        if (anim.isComplete) return 1.0f;
        uint32_t elapsed = millis() - anim.startTime;
        return min(1.0f, (float)elapsed / anim.duration);
      }
    }
    return 0.0f;
  }
  
  // Performance monitoring
  float getFPS() const { return currentFPS; }
  
  void getPerformanceStats(uint32_t& avgFrameTime, uint32_t& minTime, uint32_t& maxTime) {
    avgFrameTime = frameCount > 0 ? totalFrameTime / frameCount : 0;
    minTime = minFrameTime;
    maxTime = maxFrameTime;
  }
  
  void resetPerformanceStats() {
    totalFrameTime = 0;
    minFrameTime = UINT32_MAX;
    maxFrameTime = 0;
  }
  
private:
  void cleanupAnimations() {
    // Remove completed animations if we're running low on space
    if (animations.size() >= MAX_ANIMATIONS - 2) {
      animations.erase(
        std::remove_if(animations.begin(), animations.end(),
                       [](const Animation& a) { return a.isComplete; }),
        animations.end()
      );
    }
  }
  
  void updatePerformanceMetrics(uint32_t frameTime) {
    totalFrameTime += frameTime;
    minFrameTime = min(minFrameTime, frameTime);
    maxFrameTime = max(maxFrameTime, frameTime);
  }
};