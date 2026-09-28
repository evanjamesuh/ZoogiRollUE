export interface PremiumZoogi {
  id: string;
  name: string;
  description: string;
  color: string;
  accentColor: string;
  ability: string;
  abilityDescription: string;
  stats: {
    speed: number;
    power: number;
    defense: number;
    control: number;
  };
  price: number;
}

export const PREMIUM_CHARACTERS: PremiumZoogi[] = [
  {
    id: "blaze",
    name: "Blaze",
    description: "A fiery dragon creature with explosive power",
    color: "#FF4136",
    accentColor: "#FF851B",
    ability: "Flame Burst",
    abilityDescription: "Leaves a trail of fire that damages enemies",
    stats: { speed: 7, power: 9, defense: 5, control: 6 },
    price: 199
  },
  {
    id: "nightshade",
    name: "Nightshade",
    description: "A shadowy demon with mysterious dark powers",
    color: "#6B46C1",
    accentColor: "#9F7AEA",
    ability: "Shadow Stun",
    abilityDescription: "Stuns enemies on collision for 3 seconds",
    stats: { speed: 6, power: 7, defense: 6, control: 8 },
    price: 199
  },
  {
    id: "phantom",
    name: "Phantom",
    description: "A ghostly skull that phases through obstacles",
    color: "#E2E8F0",
    accentColor: "#90CDF4",
    ability: "Phase Shift",
    abilityDescription: "Can pass through mushrooms without rebounding",
    stats: { speed: 8, power: 5, defense: 4, control: 9 },
    price: 249
  },
  {
    id: "inferno",
    name: "Inferno",
    description: "A demonic crab with devastating impact",
    color: "#C53030",
    accentColor: "#FC8181",
    ability: "Ground Slam",
    abilityDescription: "Creates shockwaves that push nearby enemies",
    stats: { speed: 5, power: 10, defense: 8, control: 4 },
    price: 299
  },
  {
    id: "bones",
    name: "Bones",
    description: "A lightweight skeleton with quick reflexes",
    color: "#F7FAFC",
    accentColor: "#CBD5E0",
    ability: "Bone Bounce",
    abilityDescription: "Extra bouncy - rebounds at 150% normal force",
    stats: { speed: 9, power: 4, defense: 3, control: 8 },
    price: 149
  },
  {
    id: "thorn",
    name: "Thorn",
    description: "A goblin covered in sharp spikes",
    color: "#48BB78",
    accentColor: "#68D391",
    ability: "Spike Damage",
    abilityDescription: "Deals extra knockback when colliding",
    stats: { speed: 7, power: 8, defense: 5, control: 6 },
    price: 199
  },
  {
    id: "bubbles",
    name: "Bubbles",
    description: "A slippery fish that gains speed on ice",
    color: "#805AD5",
    accentColor: "#B794F4",
    ability: "Slippery",
    abilityDescription: "Gains 2x speed boost on ice patches",
    stats: { speed: 8, power: 5, defense: 5, control: 7 },
    price: 199
  },
  {
    id: "rotter",
    name: "Rotter",
    description: "A zombie that leaves a toxic trail",
    color: "#68D391",
    accentColor: "#9AE6B4",
    ability: "Toxic Trail",
    abilityDescription: "Enemies in trail take continuous damage",
    stats: { speed: 5, power: 7, defense: 7, control: 6 },
    price: 249
  },
  {
    id: "frost",
    name: "Frost",
    description: "An icy wolf that freezes enemies",
    color: "#63B3ED",
    accentColor: "#BEE3F8",
    ability: "Ice Trail",
    abilityDescription: "Creates ice patches that slow enemies",
    stats: { speed: 8, power: 6, defense: 5, control: 8 },
    price: 249
  },
  {
    id: "basher",
    name: "Basher",
    description: "A heavily armored orc warrior",
    color: "#38A169",
    accentColor: "#9AE6B4",
    ability: "Heavy Hitter",
    abilityDescription: "Knockbacks deal 2x force to enemies",
    stats: { speed: 4, power: 10, defense: 9, control: 4 },
    price: 299
  }
];

export const CHARACTER_BUNDLE = {
  id: "monster-pack",
  name: "Monster Pack",
  description: "All 11 premium characters - Save 50%!",
  price: 999,
  includes: PREMIUM_CHARACTERS.map(c => c.id)
};
