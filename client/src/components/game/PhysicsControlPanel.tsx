import { useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Settings2, Copy, RotateCcw, ChevronDown, ChevronUp, Check, Info } from "lucide-react";
import { 
  getPhysicsConfig, 
  setPhysicsValue, 
  resetPhysicsConfig, 
  exportPhysicsConfigAsCode,
  PhysicsConfig 
} from "@/lib/physicsConfig";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

const settingDescriptions: Record<keyof PhysicsConfig, string> = {
  gravity: "Downward force on all objects. Lower values = floatier gameplay, higher = heavier and faster falls.",
  fixedTimestep: "Physics simulation time step. Smaller = more precise but slower, larger = faster but less accurate.",
  maxSubsteps: "Max physics steps per frame. Higher = smoother simulation under lag, but more CPU intensive.",
  zoogiRadius: "Size of Zoogi collision sphere. Larger = easier to hit targets but harder to dodge.",
  zoogiMass: "Weight of Zoogis. Heavier = harder to knock off but slower to accelerate.",
  zoogiRestitution: "Bounciness of Zoogis. Higher = more bouncy collisions, lower = less bounce.",
  zoogiFriction: "Surface grip of Zoogis. Higher = stops faster on ground, lower = slides more.",
  zoogiLinearDamping: "Air resistance for movement. Higher = stops faster in air, lower = maintains speed longer.",
  zoogiAngularDamping: "Spin resistance. Higher = stops spinning faster, lower = keeps rotating.",
  orbRadius: "Size of collectible orbs. Larger = easier to collect but also easier to knock away.",
  orbRestitution: "Bounciness of orbs. Higher = orbs bounce more when hit.",
  orbFriction: "Ground grip of orbs. Higher = orbs stop faster when rolling.",
  orbLinearDamping: "Air drag on orbs. Higher = orbs slow down faster after being hit.",
  orbAngularDamping: "Spin resistance of orbs. Higher = orbs stop spinning faster.",
  arenaRadius: "Size of the play area. Larger = more room to maneuver, smaller = tighter battles.",
  wallThickness: "Thickness of arena walls. Affects collision detection at edges.",
  collisionZoogiExtra: "Extra collision padding between Zoogis. Higher = collisions trigger from further away.",
  collisionOrbExtra: "Extra collision padding for orbs. Higher = orbs are collected from further away.",
  knockoffYThreshold: "Y position that triggers knockout. Lower = must fall further to be eliminated.",
  knockoffRadiusExtra: "Extra distance past arena edge for knockout. Higher = more forgiving at edges.",
  movementStoppedThreshold: "Speed below which movement is considered stopped. Lower = more precise turn detection.",
  maxVelocity: "Speed limit for all entities. Higher = faster gameplay but harder to control.",
  launchImpulseMultiplier: "Power multiplier for launches. Higher = more powerful initial thrust.",
  arcLandingVelocityThreshold: "Downward speed that triggers arc landing effects. Lower = need steeper landings.",
  shockwaveRadius: "Explosion radius from arc landings. Larger = affects more nearby objects.",
  shockwaveCooldown: "Time between shockwave effects. Lower = more frequent explosions."
};

interface SliderRowProps {
  label: string;
  configKey: keyof PhysicsConfig;
  min: number;
  max: number;
  step: number;
  unit?: string;
}

function SliderRow({ label, configKey, min, max, step, unit = "" }: SliderRowProps) {
  const _forceUpdate = useZoogiGame(state => state.moveUpdateCounter);
  const [showTooltip, setShowTooltip] = useState(false);
  const config = getPhysicsConfig();
  const value = config[configKey] as number;
  const description = settingDescriptions[configKey];
  
  return (
    <div className="relative">
      <div className="flex items-center gap-2 text-xs">
        <button
          className="text-yellow-400 hover:text-yellow-300 transition-colors flex-shrink-0"
          onMouseEnter={() => setShowTooltip(true)}
          onMouseLeave={() => setShowTooltip(false)}
          onClick={() => setShowTooltip(!showTooltip)}
        >
          <Info size={12} />
        </button>
        <span className="text-white/70 w-20 truncate" title={label}>{label}</span>
        <input
          type="range"
          min={min}
          max={max}
          step={step}
          value={value}
          onChange={(e) => setPhysicsValue(configKey, parseFloat(e.target.value))}
          className="flex-1 h-1 accent-purple-500"
        />
        <span className="text-white font-mono w-14 text-right">{value.toFixed(2)}{unit}</span>
      </div>
      
      <AnimatePresence>
        {showTooltip && (
          <motion.div
            initial={{ opacity: 0, x: -10, scaleX: 0 }}
            animate={{ opacity: 1, x: 0, scaleX: 1 }}
            exit={{ opacity: 0, x: -10, scaleX: 0 }}
            transition={{ duration: 0.2, ease: "easeOut" }}
            style={{ transformOrigin: "left center" }}
            className="absolute left-0 right-0 mt-1 z-50"
          >
            <div className="bg-yellow-400 text-black text-[10px] leading-tight px-2 py-1.5 rounded shadow-lg relative overflow-hidden">
              <div 
                className="absolute left-0 top-0 bottom-0 w-1 bg-yellow-600"
                style={{ boxShadow: "2px 0 4px rgba(0,0,0,0.2)" }}
              />
              <div className="pl-1">
                <span className="font-bold">{label}:</span> {description}
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}

interface SectionProps {
  title: string;
  color: string;
  children: React.ReactNode;
  defaultOpen?: boolean;
}

function Section({ title, color, children, defaultOpen = false }: SectionProps) {
  const [isOpen, setIsOpen] = useState(defaultOpen);
  
  return (
    <div className={`border-l-2 pl-2 mb-2`} style={{ borderColor: color }}>
      <button
        onClick={() => setIsOpen(!isOpen)}
        className="flex items-center gap-1 text-xs font-semibold text-white/90 w-full"
      >
        {isOpen ? <ChevronDown size={12} /> : <ChevronUp size={12} />}
        <span style={{ color }}>{title}</span>
      </button>
      {isOpen && <div className="mt-1 space-y-2">{children}</div>}
    </div>
  );
}

export function PhysicsControlPanel() {
  const [isOpen, setIsOpen] = useState(false);
  const [copied, setCopied] = useState(false);
  const _forceUpdate = useZoogiGame(state => state.moveUpdateCounter);
  
  const handleExport = () => {
    const code = exportPhysicsConfigAsCode();
    navigator.clipboard.writeText(code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
    console.log("Physics config exported:\n", code);
  };
  
  const handleReset = () => {
    resetPhysicsConfig();
  };
  
  return (
    <div className="pointer-events-auto">
      <button
        onClick={() => setIsOpen(!isOpen)}
        className={`w-10 h-10 rounded-lg flex items-center justify-center transition-all ${
          isOpen ? "bg-purple-600" : "bg-black/60 hover:bg-black/80"
        }`}
        title="Physics Config"
      >
        <Settings2 size={18} className="text-white" />
      </button>
      
      <AnimatePresence>
        {isOpen && (
          <motion.div
            initial={{ opacity: 0, y: -10, scale: 0.95 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: -10, scale: 0.95 }}
            className="absolute top-12 right-0 w-80 bg-black/90 backdrop-blur-sm rounded-lg p-3 border border-white/20 max-h-[70vh] overflow-y-auto"
          >
            <div className="flex items-center justify-between mb-3">
              <h3 className="text-white font-bold text-sm">Physics Config</h3>
              <div className="flex gap-1">
                <button
                  onClick={handleReset}
                  className="p-1.5 rounded bg-orange-600/80 hover:bg-orange-500 transition-colors"
                  title="Reset to defaults"
                >
                  <RotateCcw size={12} className="text-white" />
                </button>
                <button
                  onClick={handleExport}
                  className={`p-1.5 rounded transition-colors flex items-center gap-1 ${
                    copied ? "bg-green-600" : "bg-blue-600/80 hover:bg-blue-500"
                  }`}
                  title="Copy config as code"
                >
                  {copied ? <Check size={12} className="text-white" /> : <Copy size={12} className="text-white" />}
                </button>
              </div>
            </div>
            
            <div className="mb-2 p-2 bg-yellow-400/10 border border-yellow-400/30 rounded text-[10px] text-yellow-300 flex items-center gap-2">
              <Info size={14} className="flex-shrink-0" />
              <span>Tap the <span className="text-yellow-400 font-bold">ⓘ</span> icons to see what each setting does</span>
            </div>
            
            <Section title="World" color="#60A5FA" defaultOpen={true}>
              <SliderRow label="Gravity" configKey="gravity" min={-20} max={-1} step={0.1} />
              <SliderRow label="Max Substeps" configKey="maxSubsteps" min={1} max={8} step={1} />
            </Section>
            
            <Section title="Zoogi" color="#34D399" defaultOpen={true}>
              <SliderRow label="Radius" configKey="zoogiRadius" min={0.1} max={10} step={0.05} />
              <SliderRow label="Mass" configKey="zoogiMass" min={0.1} max={10} step={0.1} />
              <SliderRow label="Restitution" configKey="zoogiRestitution" min={0} max={10} step={0.05} />
              <SliderRow label="Friction" configKey="zoogiFriction" min={0} max={10} step={0.05} />
              <SliderRow label="Linear Damp" configKey="zoogiLinearDamping" min={0} max={10} step={0.05} />
              <SliderRow label="Angular Damp" configKey="zoogiAngularDamping" min={0} max={10} step={0.05} />
            </Section>
            
            <Section title="Orbs" color="#FBBF24" defaultOpen={false}>
              <SliderRow label="Radius" configKey="orbRadius" min={0.1} max={10} step={0.05} />
              <SliderRow label="Restitution" configKey="orbRestitution" min={0} max={10} step={0.05} />
              <SliderRow label="Friction" configKey="orbFriction" min={0} max={10} step={0.05} />
              <SliderRow label="Linear Damp" configKey="orbLinearDamping" min={0} max={10} step={0.05} />
              <SliderRow label="Angular Damp" configKey="orbAngularDamping" min={0} max={10} step={0.05} />
            </Section>
            
            <Section title="Arena" color="#F472B6" defaultOpen={false}>
              <SliderRow label="Radius" configKey="arenaRadius" min={10} max={30} step={1} />
              <SliderRow label="Wall Thick" configKey="wallThickness" min={0.5} max={3} step={0.1} />
            </Section>
            
            <Section title="Collision" color="#A78BFA" defaultOpen={false}>
              <SliderRow label="Zoogi Extra" configKey="collisionZoogiExtra" min={0} max={10} step={0.05} />
              <SliderRow label="Orb Extra" configKey="collisionOrbExtra" min={0} max={10} step={0.05} />
              <SliderRow label="Knockoff Y" configKey="knockoffYThreshold" min={-10} max={0} step={0.5} />
              <SliderRow label="Knockoff R+" configKey="knockoffRadiusExtra" min={0} max={10} step={0.5} />
              <SliderRow label="Stop Thresh" configKey="movementStoppedThreshold" min={0.01} max={10} step={0.01} />
              <SliderRow label="Max Velocity" configKey="maxVelocity" min={5} max={100} step={1} />
            </Section>
            
            <Section title="Launch/Impact" color="#FB923C" defaultOpen={false}>
              <SliderRow label="Impulse Mult" configKey="launchImpulseMultiplier" min={1} max={50} step={0.5} />
              <SliderRow label="Arc Land Vel" configKey="arcLandingVelocityThreshold" min={-20} max={0} step={0.5} />
              <SliderRow label="Shockwave R" configKey="shockwaveRadius" min={1} max={20} step={0.5} />
              <SliderRow label="Shock CD" configKey="shockwaveCooldown" min={100} max={5000} step={100} unit="ms" />
            </Section>
            
            <div className="mt-2 pt-2 border-t border-white/10 text-white/40 text-[10px] text-center">
              Changes apply to new games. Export to save permanently.
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
