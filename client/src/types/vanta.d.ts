declare module 'vanta/dist/vanta.dots.min' {
  interface VantaDotsOptions {
    el: HTMLElement | null;
    THREE: any;
    mouseControls?: boolean;
    touchControls?: boolean;
    gyroControls?: boolean;
    minHeight?: number;
    minWidth?: number;
    scale?: number;
    scaleMobile?: number;
    color?: number;
    color2?: number;
    backgroundColor?: number;
    size?: number;
    spacing?: number;
    showLines?: boolean;
  }
  
  interface VantaEffect {
    destroy: () => void;
    resize: () => void;
  }
  
  function VANTA(options: VantaDotsOptions): VantaEffect;
  export default VANTA;
}
