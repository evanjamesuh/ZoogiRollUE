import { motion, AnimatePresence } from "framer-motion";
import { X } from "lucide-react";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

interface TutorialStep {
  title: string;
  message: string;
  icon: string;
}

const BASE_TUTORIAL_STEPS: TutorialStep[] = [
  {
    title: "Drag to Launch!",
    message: "Touch and drag your Zoogi backwards, then release to launch forward!",
    icon: "👆"
  },
  {
    title: "Power Control",
    message: "Drag further for more power! GREEN = Low, YELLOW = Medium, RED = High. Keep pulling and the label turns SLINGSHOT.",
    icon: "💪"
  },
  {
    title: "Knock Orbs Off!",
    message: "Hit the colorful orbs off the arena edge for +50 points each!",
    icon: "🎯"
  },
  {
    title: "Knock 'Em Out!",
    message: "Hit enemies at high speed to knock them off the arena! You earn +100 points per knockout.",
    icon: "💥"
  },
  {
    title: "Special Abilities",
    message: "Each Zoogi has a unique ability! Check your character panel to see what yours does.",
    icon: "⚡"
  }
];

const RINGER_ROYALE_STEPS: TutorialStep[] = [
  {
    title: "Free-for-All!",
    message: "Everyone moves at once! It's a chaotic battle royale where the highest score wins!",
    icon: "🏆"
  },
  {
    title: "Tap to Boost!",
    message: "Tap the screen while rolling to get a quick speed boost in your current direction!",
    icon: "👆"
  },
  {
    title: "Swipe to Steer!",
    message: "Swipe across the screen to add momentum in that direction. Use it to dodge or chase!",
    icon: "👋"
  },
  {
    title: "Knock Orbs Off!",
    message: "Hit the colorful orbs off the arena edge for +50 points each!",
    icon: "🎯"
  },
  {
    title: "Knock 'Em Out!",
    message: "Hit enemies at high speed to knock them off the arena! You earn +100 points per knockout.",
    icon: "💥"
  },
  {
    title: "Special Abilities",
    message: "Each Zoogi has a unique ability! Check your character panel to see what yours does.",
    icon: "⚡"
  }
];

const VOICE_CHAT_STEP: TutorialStep = {
  title: "Voice Chat",
  message: "Tap the phone icon to talk with other players! Tap again to mute/unmute yourself.",
  icon: "📞"
};

export function Tutorial() {
  const { 
    phase, 
    showTutorial, 
    tutorialStep, 
    closeTutorial, 
    nextTutorialStep,
    gameMode
  } = useZoogiGame();
  
  if (phase !== "playing" || !showTutorial) {
    return null;
  }
  
  const isRingerRoyale = gameMode === "ringer_royale";
  const isTeamMode = gameMode === "local_multiplayer" || gameMode === "ringer_royale";
  
  let tutorialSteps: TutorialStep[];
  if (isRingerRoyale) {
    tutorialSteps = [...RINGER_ROYALE_STEPS, VOICE_CHAT_STEP];
  } else if (isTeamMode) {
    tutorialSteps = [...BASE_TUTORIAL_STEPS, VOICE_CHAT_STEP];
  } else {
    tutorialSteps = BASE_TUTORIAL_STEPS;
  }
  
  const currentStep = tutorialSteps[tutorialStep];
  const isLastStep = tutorialStep >= tutorialSteps.length - 1;
  
  const handleTap = () => {
    if (isLastStep) {
      closeTutorial();
    } else {
      nextTutorialStep();
    }
  };
  
  if (!currentStep) {
    closeTutorial();
    return null;
  }
  
  return (
    <AnimatePresence>
      <motion.div
        key={tutorialStep}
        initial={{ opacity: 0, y: 50 }}
        animate={{ opacity: 1, y: 0 }}
        exit={{ opacity: 0, y: -50 }}
        className="fixed bottom-32 left-1/2 transform -translate-x-1/2 z-50 pointer-events-auto"
        onClick={handleTap}
      >
        <div className="bg-black/80 backdrop-blur-sm rounded-2xl px-6 py-4 max-w-sm border border-white/20 shadow-2xl relative">
          <button
            onClick={(e) => {
              e.stopPropagation();
              closeTutorial();
            }}
            className="absolute top-2 right-2 p-1 text-white/50 hover:text-white"
          >
            <X size={18} />
          </button>
          <div className="flex items-start gap-3 pr-6">
            <span className="text-3xl">{currentStep.icon}</span>
            <div className="flex-1">
              <h3 className="text-white font-bold text-lg mb-1">
                {currentStep.title}
              </h3>
              <p className="text-white/80 text-sm leading-relaxed">
                {currentStep.message}
              </p>
            </div>
          </div>
          <div className="mt-3 flex justify-between items-center">
            <span className="text-white/40 text-xs">
              {tutorialStep + 1} / {tutorialSteps.length}
            </span>
            <span className="text-white/50 text-xs">
              {isLastStep ? "Tap to close" : "Tap for next"}
            </span>
          </div>
        </div>
      </motion.div>
    </AnimatePresence>
  );
}
