# Extension infrastructure

This fork retains the original Node.js extension infrastructure, but extension
SDK development is outside its current feature scope. There is no maintained
extension API reference or compatibility guarantee for third-party extensions.

Implementation references:

- [Extension loading and server](../../src/ui/Extensions/)
- [Exported API stubs](../../src/ui/Extensions/Stubs/)
- [Extension packaging tools](../../src/extension_tools/)

The fork's comment, tab and comparison features are built into the application;
they do not require extensions. See the [feature guide](../../docs/FEATURES.md)
and [architecture](../../docs/ARCHITECTURE.md).
