# HE3D Documentation Index

[中文版](index.md)

This is the entry point for HE3D documentation. Start with Getting Started, read Concepts for the mental model, and use the Porting Guide when writing a new backend. The complete API manual is maintained in [Technical Overview](technical.en.md); bilingual comments in `include/` are implementation-side hints and the final declaration source.

## Reading Order

1. [Getting Started](getting-started.en.md): build, run examples, and write the first HE3D program.
2. [Core Concepts](concepts.en.md): understand Platform, Renderer, Mesh/GameObject, and PhysicsScene.
3. [Examples](examples.en.md): what each example demonstrates and how to read the source.
4. [API Guide](api-guide.en.md): common API call paths and boundaries by module.
5. [Porting Guide](porting.en.md): implement a HE3D backend for a new platform.
6. [Technical Overview](technical.en.md): repository structure, module boundaries, and maintenance rules.

## Maintenance Rules

- Chinese and English documents are maintained as pairs with matching heading structure where practical.
- Documents explain how and why to use APIs, and the Technical Overview maintains a complete public API reference.
- API changes update `include/` comments first, then the relevant guides and examples.
- Avoid copying large class declarations into documents so docs do not drift from headers.
