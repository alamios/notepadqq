# Contributing

This fork is maintained by **alamios** at
[alamios/notepadqq](https://github.com/alamios/notepadqq). Submit issues and pull
requests there rather than to the original project's tracker.

## Bug reports

Search existing [issues](https://github.com/alamios/notepadqq/issues) before
opening a report. Include:

- Fork commit or version and installation method.
- OS, desktop/session type, Qt version and editor backend.
- Reproduction steps, expected result and actual result.
- Relevant sanitized logs or screenshots, and whether unsaved data is affected.

Distinguish an observed fault from a suspected cause. Reproduce session problems
with an isolated profile and disposable or copied documents. Do not delete real
session caches to investigate a failure. Reports about an original distribution
package should identify it explicitly.

## Changes

Discuss substantial features before implementation. Keep changes focused, follow
the surrounding code's conventions, and write code, comments, documentation and
commit messages in English. Add regression coverage where it verifies meaningful
behavior, and update the README and affected guides.

Create branches from `main` and target pull requests to `alamios/notepadqq:main`.
The `upstream-baseline` tag records the unmodified starting point and must not
be moved. Remote names and publication are managed by the maintainer.

See [development](docs/DEVELOPMENT.md) for build, test and profile-isolation
instructions, and [architecture](docs/ARCHITECTURE.md) for implementation context.
Run the checks relevant to the change, report what was tested and document any
remaining limitations. UI changes may also need native desktop review.

When integrating an approved branch from a clean checkout:

```sh
git switch main
git merge --ff-only <feature-branch>
```

If histories diverge, review the divergence before choosing another merge
strategy. Preserve original attribution and third-party licenses. Community
participation follows the [Code of Conduct](CODE_OF_CONDUCT.md).
