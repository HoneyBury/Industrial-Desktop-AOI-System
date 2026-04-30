# Development Workflow

## End-to-End Flow

Issue -> feature branch -> commit -> Pull Request -> CI -> Code Review -> merge to `develop` ->
release branch -> merge to `main` -> tag release

## Working Rules

1. Start with a GitHub Issue or refine an existing one.
2. Branch from `develop`.
3. Keep changes scoped to one purpose.
4. Use Conventional Commits.
5. Add tests for logic changes.
6. Pass CI before merge.
7. Update docs for architecture, interface, or process changes.

## Pull Request Expectations

- describe business purpose
- summarize technical changes
- provide test evidence
- call out risks and compatibility impact
- include screenshots or video for UI-visible changes

