import fs from "fs/promises";

export class ParsedRadixNode {
  public fragment: string; // The string fragment for this node
  public payload: number | null; // Payload (e.g., ID) for this node
  public children: ParsedRadixNode[]; // Children nodes as ParsedRadixNode objects
  constructor(
    fragment: string,
    payload: number,
    children: ParsedRadixNode[] = []
  ) {
    this.fragment = fragment;
    this.payload = payload;
    this.children = children;
  }
}

export class ParsedRadixTree {
  root: ParsedRadixNode; // Root node of the parsed tree
  constructor(root: ParsedRadixNode) {
    this.root = root;
  }
}

export class RadixNode {
  fragment: string; // The string fragment for this node
  payload: number | null; // Payload (e.g., ID) for this node,
  children: Map<string, RadixNode>; // Children nodes keyed by their fragment
  offset: number; // Index in the node array for binary output
  constructor(fragment: string, payload = null) {
    this.fragment = fragment;
    this.payload = payload;
    this.children = new Map();
    this.offset = 0; // Index in node array
  }
}

export class RadixTreeBuilder {
  private root: RadixNode;
  private stringPool: Map<string, number>; // fragment -> offset
  private stringBuffer: string[]; // Buffer for string fragments
  private nodeList: RadixNode[]; // List of nodes for final binary output
  constructor() {
    this.root = new RadixNode("");
    this.stringPool = new Map(); // fragment -> offset
    this.stringBuffer = [];
    this.nodeList = [];
  }

  add(word: string, payload: number) {
    let node = this.root;
    let i = 0;
    while (i < word.length) {
      for (const [frag, child] of node.children) {
        const common = this.commonPrefix(frag, word.slice(i));
        if (common.length > 0) {
          if (common.length < frag.length) {
            // Split existing child
            const oldChild = child;
            const newChild = new RadixNode(frag.slice(common.length));
            newChild.children = oldChild.children;
            newChild.payload = oldChild.payload;

            oldChild.fragment = common;
            oldChild.children = new Map([[newChild.fragment, newChild]]);
            oldChild.payload = null;
          }
          node = child;
          i += common.length;
          continue;
        }
      }
      const newFrag = word.slice(i);
      const newNode = new RadixNode(newFrag, payload);
      node.children.set(newFrag, newNode);
      return;
    }
    node.payload = payload;
  }

  commonPrefix(a: string, b: string) {
    let i = 0;
    while (i < a.length && i < b.length && a[i] === b[i]) i++;
    return a.slice(0, i);
  }

  finalize() {
    this.nodeList = [];
    this._assignOffsets(this.root);
  }

  _assignOffsets(node: RadixNode) {
    for (const child of node.children.values()) {
      this._assignOffsets(child);
    }
    node.offset = this.nodeList.length;
    this.nodeList.push(node);
  }

  buildBinary() {
    const stringPoolOffsets = new Map();
    let stringPoolBuf: Buffer[] = [];
    let offset = 0;

    const getStrOffset = (frag: string) => {
      if (stringPoolOffsets.has(frag)) return stringPoolOffsets.get(frag);
      const buf = Buffer.from(frag, "utf8");
      stringPoolBuf.push(buf);
      stringPoolOffsets.set(frag, offset);
      const thisOffset = offset;
      offset += buf.length;
      return thisOffset;
    };

    const nodes = this.nodeList;
    const nodeBuf = Buffer.alloc(nodes.length * 16);

    for (let i = 0; i < nodes.length; i++) {
      const n = nodes[i];
      const fragOffset = getStrOffset(n.fragment);

      const children = [...n.children.values()];
      const childrenOffset = children.length > 0 ? children[0].offset : 0;

      nodeBuf.writeUInt16LE(fragOffset, i * 16 + 0);
      nodeBuf.writeUInt8(n.fragment.length, i * 16 + 2);
      nodeBuf.writeUInt8(children.length, i * 16 + 3);
      nodeBuf.writeUInt32LE(childrenOffset, i * 16 + 4);
      nodeBuf.writeUInt32LE(n.payload ?? 0xffffffff, i * 16 + 8);
      nodeBuf.writeUInt16LE(0, i * 16 + 12); // flags
      nodeBuf.writeUInt16LE(0, i * 16 + 14); // padding/reserved
    }

    const stringBuf = Buffer.concat(stringPoolBuf);
    const header = Buffer.alloc(12);
    header.writeUInt32LE(this.root.offset, 0);
    header.writeUInt32LE(nodes.length, 4);
    header.writeUInt32LE(stringBuf.length, 8);

    return Buffer.concat([header, nodeBuf, stringBuf]);
  }

  async saveToFile(filePath: string) {
    const buf = this.buildBinary();
    await fs.writeFile(filePath, buf);
    console.log(`Written ${buf.length} bytes to ${filePath}`);
  }
}

export async function parse(filePath: string): Promise<ParsedRadixTree> {
  const data = await fs.readFile(filePath);

  const rootIndex = data.readUInt32LE(0);
  const nodeCount = data.readUInt32LE(4);
  const stringPoolSize = data.readUInt32LE(8);

  const nodes = [];
  const stringPoolOffset = 12 + nodeCount * 16;
  const stringPool = data.slice(
    stringPoolOffset,
    stringPoolOffset + stringPoolSize
  );

  // Lire tous les nœuds
  for (let i = 0; i < nodeCount; i++) {
    const offset = 12 + i * 16;
    const strOffset = data.readUInt16LE(offset + 0);
    const strLen = data.readUInt8(offset + 2);
    const numChildren = data.readUInt8(offset + 3);
    const childrenOffset = data.readUInt32LE(offset + 4);
    const payload = data.readUInt32LE(offset + 8);
    const flags = data.readUInt16LE(offset + 12);

    const fragment = stringPool
      .slice(strOffset, strOffset + strLen)
      .toString("utf8");

    nodes.push({
      fragment,
      payload: payload !== 0xffffffff ? payload : null,
      numChildren,
      childrenOffset,
      flags,
    });
  }

  // Convertir en arbre d’objets
  const nodeObjs = new Array(nodeCount).fill(null);
  for (let i = nodeCount - 1; i >= 0; i--) {
    const n = nodes[i];
    const children: ParsedRadixNode[] = [];
    for (let j = 0; j < n.numChildren; j++) {
      children.push(nodeObjs[n.childrenOffset + j]);
    }
    nodeObjs[i] = new ParsedRadixNode(n.fragment, n.payload, children);
  }

  return new ParsedRadixTree(nodeObjs[rootIndex]);
}
