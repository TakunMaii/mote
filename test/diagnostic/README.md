Diagnostic-focused negative examples for validating compiler error messages.

Files in this directory are intended to fail compilation and exercise lexer, parser,
semantic, and type-system diagnostics.

The minimal harness also includes:

- generated scope-capacity overflow cases
- MIR lowering diagnostics
- LLVM backend diagnostics

Generic-specific diagnostics in this directory cover current failure modes around:

- explicit type-argument arity mismatches
- failed or conflicting generic inference
- reference/optional/slice element mismatches after specialization
- cross-module visibility errors for generic/type-factory exports
