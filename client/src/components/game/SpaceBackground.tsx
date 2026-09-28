import * as THREE from "three";
import { useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";

interface Star {
  position: [number, number, number];
  size: number;
  brightness: number;
}

interface ShootingStar {
  startPos: [number, number, number];
  direction: [number, number, number];
  speed: number;
  length: number;
  active: boolean;
  progress: number;
}

export function SpaceBackground() {
  const starsRef = useRef<THREE.Points>(null);
  const shootingStarsRef = useRef<ShootingStar[]>([]);
  const shootingStarMeshRefs = useRef<(THREE.Mesh | null)[]>([]);
  const nebulaRef = useRef<THREE.Mesh>(null);
  
  const stars = useMemo<Star[]>(() => {
    const result: Star[] = [];
    const starCount = 800;
    
    for (let i = 0; i < starCount; i++) {
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.acos(2 * Math.random() - 1);
      const radius = 80 + Math.random() * 40;
      
      result.push({
        position: [
          radius * Math.sin(phi) * Math.cos(theta),
          radius * Math.cos(phi),
          radius * Math.sin(phi) * Math.sin(theta)
        ],
        size: 0.1 + Math.random() * 0.2,
        brightness: 0.5 + Math.random() * 0.5
      });
    }
    return result;
  }, []);
  
  const starGeometry = useMemo(() => {
    const geometry = new THREE.BufferGeometry();
    const positions = new Float32Array(stars.length * 3);
    const sizes = new Float32Array(stars.length);
    
    stars.forEach((star, i) => {
      positions[i * 3] = star.position[0];
      positions[i * 3 + 1] = star.position[1];
      positions[i * 3 + 2] = star.position[2];
      sizes[i] = star.size;
    });
    
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute('size', new THREE.BufferAttribute(sizes, 1));
    
    return geometry;
  }, [stars]);
  
  useMemo(() => {
    shootingStarsRef.current = Array(5).fill(null).map(() => ({
      startPos: [0, 0, 0] as [number, number, number],
      direction: [0, 0, 0] as [number, number, number],
      speed: 0,
      length: 0,
      active: false,
      progress: 0
    }));
  }, []);
  
  useFrame((state, delta) => {
    if (starsRef.current) {
      starsRef.current.rotation.y += delta * 0.005;
    }
    
    shootingStarsRef.current.forEach((star, i) => {
      if (!star.active) {
        if (Math.random() < 0.002) {
          const theta = Math.random() * Math.PI * 2;
          const phi = Math.PI / 4 + Math.random() * Math.PI / 2;
          const radius = 60;
          
          star.startPos = [
            radius * Math.sin(phi) * Math.cos(theta),
            30 + Math.random() * 30,
            radius * Math.sin(phi) * Math.sin(theta)
          ];
          
          const targetTheta = theta + Math.PI + (Math.random() - 0.5) * 0.5;
          star.direction = [
            Math.cos(targetTheta) * 0.7,
            -0.3 - Math.random() * 0.2,
            Math.sin(targetTheta) * 0.7
          ];
          
          star.speed = 30 + Math.random() * 20;
          star.length = 3 + Math.random() * 4;
          star.active = true;
          star.progress = 0;
        }
      } else {
        star.progress += delta * star.speed;
        
        if (star.progress > 100) {
          star.active = false;
        }
      }
      
      const mesh = shootingStarMeshRefs.current[i];
      if (mesh) {
        if (star.active) {
          const pos: [number, number, number] = [
            star.startPos[0] + star.direction[0] * star.progress,
            star.startPos[1] + star.direction[1] * star.progress,
            star.startPos[2] + star.direction[2] * star.progress
          ];
          mesh.position.set(pos[0], pos[1], pos[2]);
          mesh.lookAt(
            pos[0] + star.direction[0],
            pos[1] + star.direction[1],
            pos[2] + star.direction[2]
          );
          mesh.visible = true;
        } else {
          mesh.visible = false;
        }
      }
    });
    
    if (nebulaRef.current) {
      nebulaRef.current.rotation.z += delta * 0.01;
    }
  });

  return (
    <group>
      <points ref={starsRef} geometry={starGeometry}>
        <pointsMaterial
          color="#FFFFFF"
          size={0.3}
          sizeAttenuation
          transparent
          opacity={0.9}
        />
      </points>
      
      {[0, 1, 2, 3, 4].map((i) => (
        <mesh
          key={`shooting-${i}`}
          ref={(el) => { shootingStarMeshRefs.current[i] = el; }}
          visible={false}
        >
          <cylinderGeometry args={[0.05, 0.02, 4, 8]} />
          <meshBasicMaterial color="#FFFFFF" transparent opacity={0.8} />
        </mesh>
      ))}
      
      <mesh ref={nebulaRef} position={[0, 20, -60]}>
        <planeGeometry args={[100, 100]} />
        <meshBasicMaterial
          color="#4B0082"
          transparent
          opacity={0.15}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh position={[40, 30, -50]}>
        <planeGeometry args={[60, 40]} />
        <meshBasicMaterial
          color="#8B008B"
          transparent
          opacity={0.1}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh position={[-30, 40, -40]}>
        <planeGeometry args={[50, 50]} />
        <meshBasicMaterial
          color="#00CED1"
          transparent
          opacity={0.08}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <ambientLight intensity={0.3} color="#4B0082" />
    </group>
  );
}

export function SpaceWeather() {
  const particlesRef = useRef<THREE.Points>(null);
  
  const particles = useMemo(() => {
    const geometry = new THREE.BufferGeometry();
    const count = 200;
    const positions = new Float32Array(count * 3);
    
    for (let i = 0; i < count; i++) {
      positions[i * 3] = (Math.random() - 0.5) * 60;
      positions[i * 3 + 1] = Math.random() * 30;
      positions[i * 3 + 2] = (Math.random() - 0.5) * 60;
    }
    
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    return geometry;
  }, []);
  
  useFrame((_, delta) => {
    if (particlesRef.current) {
      const positions = particlesRef.current.geometry.attributes.position.array as Float32Array;
      for (let i = 0; i < positions.length; i += 3) {
        positions[i + 1] -= delta * 0.5;
        if (positions[i + 1] < 0) {
          positions[i + 1] = 30;
        }
      }
      particlesRef.current.geometry.attributes.position.needsUpdate = true;
    }
  });

  return (
    <group>
      <points ref={particlesRef} geometry={particles}>
        <pointsMaterial
          color="#9370DB"
          size={0.1}
          transparent
          opacity={0.6}
          sizeAttenuation
        />
      </points>
    </group>
  );
}
