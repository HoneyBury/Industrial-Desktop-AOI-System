# Git Branch Strategy

## Branch Roles

- `main`: stable release branch
- `develop`: daily integration branch
- `feature/*`: feature development branch
- `bugfix/*`: defect fix branch
- `release/*`: release preparation branch
- `hotfix/*`: emergency production fix branch

## Merge Direction

- `feature/*` -> `develop`
- `bugfix/*` -> `develop`
- `release/*` -> `main` and back-merge to `develop`
- `hotfix/*` -> `main` and back-merge to `develop`

## Release Flow

1. Finish features on `develop`
2. Create `release/*`
3. Freeze scope and perform regression checks
4. Merge to `main`
5. Create `v*` tag
6. Back-merge release changes to `develop`

