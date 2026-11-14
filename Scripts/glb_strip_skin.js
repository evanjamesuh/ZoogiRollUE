const { NodeIO } = require("@gltf-transform/core");
const { prune, dedup } = require("@gltf-transform/functions");
const path = require("path");

async function main() {
  const input = process.argv[2];
  const output = process.argv[3];
  const io = new NodeIO();
  const doc = await io.read(input);
  const root = doc.getRoot();

  // Remove animations first
  for (const anim of root.listAnimations()) anim.dispose();

  // Strip skinning attributes from all primitives
  for (const mesh of root.listMeshes()) {
    for (const prim of mesh.listPrimitives()) {
      for (const sem of ["JOINTS_0", "JOINTS_1", "WEIGHTS_0", "WEIGHTS_1"]) {
        const acc = prim.getAttribute(sem);
        if (acc) {
          prim.setAttribute(sem, null);
          // dispose later via prune if unused
        }
      }
    }
  }

  // Detach skins from nodes and dispose skins
  for (const node of root.listNodes()) {
    if (node.getSkin()) node.setSkin(null);
  }
  for (const skin of root.listSkins()) skin.dispose();

  await doc.transform(prune(), dedup());
  await io.write(output, doc);
  console.log("WROTE", output);
}
main().catch((e) => { console.error(e); process.exit(1); });
