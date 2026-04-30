# INITIAL IDEA

implementation language: Typescript 

# The main structure for project (can be partial for only some part of the project)
PJTree:
- Dir: `"root"`
  - Dir: `"module1"`
    - File [source-code]: `"src1.cpp"`
      - symbol: `"Class"`
        - symbol: `"method1"`
      - symbol: `"OtherClass"`
    - File [md]: `"docs.md"`
      - header
        - list
    - File ["raw"]:
      - range: `1-10`
      - range: `11-21`
      - range: `40-43`
  - Dir: `"module2/src"`
    - File [src]: `"impl/code.ts"`
      - symbol


# something that collects data for PJTree (discovery mechanism)
Providers:
  grep: (pattern, path) -> RangeData: {uri: "root/module1", content: "class Class", range: {start, end}};
  glob: (pattern, path) -> FileData: {uri: "root/module1"};
  lsp.symbol_search: (pattern) -> SymbolData: {uri: "root/module1", kind: "class", name: "Class"};


# something that enriches PJTree from inside PJTree itself
you can make requests from specific node types:
for example:
symbol.lsp.outgoingCalls() -> ReferenceData (can be resolved to SymbolData)
directory.grep(pattern) -> RangeData

# PJTree Cursor mechanic:
knows type and methods that can be called from here
contains (or evals) some aggregation information for subtree
future feature: supports search mechanism for subnodes 




# PJTree: Structural Project Graph Specification

This document defines the architecture for a **Structural Project Graph (PJTree)**. It is a strictly typed, constraint-based tree where nodes represent physical or semantic units (files, symbols, ranges) with built-in discovery and enrichment capabilities.

---

## 1. Vision & Philosophy
The PJTree moves beyond simple file paths to represent a codebase as a **Semantic Hierarchy**.

- **Type Safety**: Nodes are governed by a strict schema. A "Raw File" node cannot contain "Symbol" children, only "Range" children.
- **On-Demand Enrichment**: The tree starts sparse. As a **Cursor** moves through the tree, **Providers** (Grep, LSP, Parsers) are triggered to populate sub-nodes.
- **Contextual Capabilities**: A node’s available actions (e.g., `outgoingCalls()`) are determined by its type at compile-time using TypeScript's specialized `this` types.

---

## 2. Technical Architecture

### A. The Schema & Constraints
The `NodeSchema` defines the data payload for every node, while `AllowedChildren` defines the "rules of the universe" for the hierarchy.

```typescript
export type Range = { start: number; end: number };

/**
 * Data shapes for every possible node type.
 * Adding a new file type (e.g., XML) starts here.
 */
export interface NodeSchema {
  "root":      { projectPath: string };
  "dir":       { name: string; path: string };
  "file-cpp":  { uri: string; language: 'cpp' };
  "file-md":   { uri: string; headers: string[] };
  "file-raw":  { uri: string };
  "symbol":    { name: string; kind: string; range?: Range };
  "range":     { start: number; end: number; content?: string };
  "md-header": { text: string; level: number };
}

type NodeKind = keyof NodeSchema;

/**
 * Hierarchical Constraints:
 * Maps a Parent Kind to the Kinds of children it is allowed to host.
 */
type AllowedChildren = {
  "root":      "dir" | "file-cpp" | "file-md" | "file-raw";
  "dir":       "dir" | "file-cpp" | "file-md" | "file-raw";
  "file-cpp":  "symbol" | "range";
  "file-raw":  "range";
  "file-md":   "md-header" | "range";
  "symbol":    "symbol" | "range"; 
  "md-header": "range";
  "range":     never;
};
```

### B. The PJNode (The Tree Unit)
The node uses generics to ensure that the `data` property matches the `kind`, and the `addChild` method enforces the hierarchy rules.

```typescript
export class PJNode<K extends NodeKind> {
  public children: PJNode<any>[] = [];

  constructor(
    public readonly kind: K,
    public data: NodeSchema[K],
    public uri: string
  ) {}

  /**
   * Type-safe child addition. 
   * CK must be a valid child for parent K according to AllowedChildren.
   */
  addChild<CK extends AllowedChildren[K]>(
    kind: CK, 
    data: NodeSchema[CK]
  ): PJNode<CK> {
    const child = new PJNode(kind, data, this.uri);
    this.children.push(child);
    return child;
  }
}
```

### C. Discovery & Providers
Providers are decoupled from the tree logic. They accept a node and "enrich" it by adding children discovered via external tools (LSP, Grep, etc.).

```typescript
export interface IProvider<K extends NodeKind> {
  provide(node: PJNode<K>): Promise<void>;
}

// Example: Provider that uses LSP to find symbols in C++ files
export const CppSymbolProvider: IProvider<"file-cpp"> = {
  provide: async (node) => {
    // 1. Logic to call LSP search
    // 2. Map results back to the tree
    node.addChild("symbol", { name: "Parser", kind: "class" });
  }
};
```

### D. The Cursor Mechanic
The Cursor provides the navigation layer and aggregates data for subtrees.

```typescript
export class PJCursor<K extends NodeKind> {
  constructor(public node: PJNode<K>) {}

  // Targeted methods: Only callable when the cursor is on a "symbol" node
  async getOutgoingCalls(this: PJCursor<"symbol">) {
    console.log(`Fetching calls for ${this.node.data.name}`);
    // Provider implementation...
  }

  // Aggregation: Recursively collect all ranges in the subtree
  collectRanges(): Range[] {
    const results: Range[] = [];
    const walk = (n: PJNode<any>) => {
      if (n.kind === "range") results.push(n.data);
      n.children.forEach(walk);
    };
    walk(this.node);
    return results;
  }
}
```

---

## 3. Usage Example

```typescript
// Initialize Root
const root = new PJNode("root", { projectPath: "/project" }, "/project");

// Implementation of the "module1" structure
const mod1 = root.addChild("dir", { name: "module1", path: "/project/module1" });

// 1. C++ File with Symbols
const src1 = mod1.addChild("file-cpp", { uri: "/project/module1/src1.cpp", language: 'cpp' });
const classNode = src1.addChild("symbol", { name: "Class", kind: "class" });
classNode.addChild("symbol", { name: "method1", kind: "method" });

// 2. Raw File with Ranges
const raw = mod1.addChild("file-raw", { uri: "/project/module1/raw" });
raw.addChild("range", { start: 1, end: 10 });
raw.addChild("range", { start: 11, end: 21 });

// 3. Navigation & Aggregation
const cursor = new PJCursor(raw);
const allRanges = cursor.collectRanges(); // Returns [ {1-10}, {11-21} ]
```

---

## 4. Key Strengths
1. **Constraint Enforcement**: Prevents invalid tree structures (e.g., adding a symbol to a raw range file) at compile-time.
2. **Modular Discovery**: Easily attach different providers for Markdown, XML, or JSON without changing the core tree logic.
3. **Fluent Navigation**: The Cursor provides a consistent API for searching and summarizing the project, regardless of the underlying file formats.
