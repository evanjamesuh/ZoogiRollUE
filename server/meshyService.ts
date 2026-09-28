const MESHY_API_BASE = "https://api.meshy.ai";

export interface MeshyTask {
  id: string;
  status: "PENDING" | "IN_PROGRESS" | "SUCCEEDED" | "FAILED" | "EXPIRED";
  progress: number;
  created_at: number;
  started_at?: number;
  finished_at?: number;
  model_urls?: {
    glb?: string;
    fbx?: string;
    usdz?: string;
    obj?: string;
  };
  thumbnail_url?: string;
  texture_urls?: {
    base_color?: string;
    metallic?: string;
    normal?: string;
    roughness?: string;
  }[];
  error?: {
    message: string;
  };
}

export interface CreateImageTo3DRequest {
  image_url?: string;
  image_data_url?: string;
  enable_pbr?: boolean;
  surface_mode?: "hard" | "organic";
  should_remesh?: boolean;
  topology?: "quad" | "triangle";
  target_polycount?: number;
}

export interface CreateImageTo3DResponse {
  result: string;
}

function getApiKey(): string {
  const apiKey = process.env.MESHY_API_KEY;
  if (!apiKey) {
    throw new Error("MESHY_API_KEY environment variable is not set");
  }
  return apiKey;
}

export async function createImageTo3DTask(
  request: CreateImageTo3DRequest
): Promise<CreateImageTo3DResponse> {
  const apiKey = getApiKey();
  
  const imageUrl = request.image_data_url || request.image_url;
  
  const response = await fetch(`${MESHY_API_BASE}/openapi/v1/image-to-3d`, {
    method: "POST",
    headers: {
      "Authorization": `Bearer ${apiKey}`,
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      image_url: imageUrl,
      enable_pbr: request.enable_pbr ?? true,
      should_remesh: request.should_remesh ?? true,
      topology: request.topology ?? "triangle",
      target_polycount: request.target_polycount ?? 30000,
    }),
  });

  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(`Meshy API error: ${response.status} - ${errorText}`);
  }

  return response.json();
}

export async function getImageTo3DTask(taskId: string): Promise<MeshyTask> {
  const apiKey = getApiKey();
  
  const response = await fetch(`${MESHY_API_BASE}/openapi/v1/image-to-3d/${taskId}`, {
    method: "GET",
    headers: {
      "Authorization": `Bearer ${apiKey}`,
    },
  });

  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(`Meshy API error: ${response.status} - ${errorText}`);
  }

  return response.json();
}

export async function downloadModel(modelUrl: string): Promise<ArrayBuffer> {
  const response = await fetch(modelUrl);
  
  if (!response.ok) {
    throw new Error(`Failed to download model: ${response.status}`);
  }

  return response.arrayBuffer();
}

export async function listImageTo3DTasks(
  pageNum: number = 1,
  pageSize: number = 10
): Promise<MeshyTask[]> {
  const apiKey = getApiKey();
  
  const response = await fetch(
    `${MESHY_API_BASE}/openapi/v1/image-to-3d?page_num=${pageNum}&page_size=${pageSize}`,
    {
      method: "GET",
      headers: {
        "Authorization": `Bearer ${apiKey}`,
      },
    }
  );

  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(`Meshy API error: ${response.status} - ${errorText}`);
  }

  return response.json();
}
