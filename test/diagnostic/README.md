Diagnostic-focused negative examples for validating compiler error messages.

Files in this directory are intended to fail compilation and exercise lexer, parser,
semantic, and type-system diagnostics.

The minimal harness also includes:

- generated scope-capacity overflow cases
- MIR lowering diagnostics
- LLVM backend diagnostics
