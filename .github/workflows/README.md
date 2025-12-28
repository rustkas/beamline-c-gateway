# C-Gateway GitHub Actions

This directory contains GitHub Actions workflows for the C-Gateway project.

## Workflows

### CI Workflow (`ci.yml`)
**Trigger**: Push/PR to main, master, develop branches  
**Jobs**:
- **build-and-test**
  - Build Gateway
  - Run unit tests (observability)
  - Run integration tests (health)
  - Run performance tests
  - Upload test results
  
- **coverage**
  - Build with coverage
  - Generate coverage reports (lcov)
  - Upload HTML coverage reports

### Release Workflow (`release.yml`)
**Trigger**: Git tags (`v*.*.*`)  
**Jobs**:
- Build release version
- Create release archive
- Create GitHub Release with binary

## Local Testing

```bash
# Build
make clean
make

# Run tests
make test-observability
make test-health
make test-performance

# Coverage
make test-coverage
bash scripts/generate_coverage.sh
```

## Artifacts

All test results and coverage reports are uploaded as artifacts:
- **test-results**: Test logs and results (7 days retention)
- **coverage-html**: HTML coverage report (7 days retention)  
- **coverage-data**: lcov and XML coverage data (7 days retention)

## Viewing Results

1. Go to **Actions** tab on GitHub
2. Select a workflow run
3. View job details and logs
4. Download artifacts if needed

## Making a Release

```bash
# Tag a new version
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0

# GitHub Actions will automatically:
# 1. Build the release
# 2. Create archive
# 3. Publish GitHub Release
```
