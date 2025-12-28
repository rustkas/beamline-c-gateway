# C-Gateway GitHub Actions - Quick Start 🚀

## What We Have

✅ **GitHub Actions CI/CD** for C-Gateway  
✅ **2 Workflows** ready to use  
✅ **Automated Testing & Coverage**

## Workflows

### 1. 🔄 **CI** (`ci.yml`)
**Runs on**: Every push & PR  
**Tests**: Build, Unit, Integration, Performance  
**Coverage**: Full lcov coverage reports  
**Time**: ~5-10 minutes

### 2. 📦 **Release** (`release.yml`)
**Runs on**: Git tags (`v*.*.*`)  
**Builds**: Release binary + archive  
**Publishes**: GitHub Release automatically

## Quick Commands

```bash
# View workflows
ls -la .github/workflows/

# Run tests locally
make test-observability
make test-health  
make test-performance

# Generate coverage
make test-coverage
bash scripts/generate_coverage.sh

# Create a release
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

## What Runs When?

| Event | Workflow Triggered |
|-------|-------------------|
| **Push to main/master/develop** | CI (build + tests) |
| **Pull Request** | CI (build + tests) |
| **Git tag v*.*.*` | Release workflow |

## Viewing Results

1. Go to **GitHub Actions** tab
2. Click on a workflow run
3. See build/test results
4. Download artifacts (test results, coverage)

## File Structure

```
.github/
└── workflows/
    ├── ci.yml          # Main CI workflow
    ├── release.yml     # Release automation
    └── README.md       # This file
```

## Next Steps

1. ✅ Commit `.github/` folder
2. ✅ Push to GitHub
3. ✅ Check Actions tab
4. 📦 Make a release tag when ready

## Status

✅ **READY TO USE**

GitHub Actions for C-Gateway is configured and ready!
