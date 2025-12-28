# Documentation Structure

This document defines the canonical documentation structure for the IPC Gateway project.

---

## Documentation Organization Rules

### Rule 1: Core Documents in Project Root

**Location**: `/` (project root)

**Files**:
- `README.md` - Project overview and quick start
- `CONTRIBUTING.md` - Contribution guidelines
- `SECURITY.md` - Security policy
- `LICENCE.md` - License file
- `CHANGELOG.md` - Version history (if exists)

**Purpose**: Essential project metadata and onboarding

---

### Rule 2: Technical Documentation in `/docs/`

**Location**: `/docs/` and subdirectories

**Structure**:
```
docs/
├── readiness/           # Production readiness assessments
│   ├── MANAGEMENT_DECISION.md
│   └── SOURCE_OF_TRUTH.md
├── validation/          # Test results and validation reports
│   ├── EVIDENCE_PACK_SYSTEM.md
│   └── (validation reports)
├── operations/          # Operational guides
│   ├── TESTING_GUIDE.md
│   └── (deployment guides)
├── architecture/        # Architecture documents
├── contracts/           # API contracts and specs
└── V2_README.md         # Version-specific docs
```

---

### Rule 3: Validation Reports in `.ai/`

**Location**: `/.ai/`

**Purpose**: Working documents, status updates, internal tracking

**Retention**: Keep only active/recent reports

**Cleanup**: Archive or remove outdated status documents

---

### Rule 4: Artifacts in `/artifacts/`

**Location**: `/artifacts/` and subdirectories

**Structure**:
```
artifacts/
├── sanitizers/          # ASan, Valgrind results
├── sanitizers-strict/   # Strict mode sanitizer results
├── soak/                # Soak test results
└── router-tests/        # Integration test results
```

**Purpose**: Test outputs, logs, evidence

---

## Migration Rules

### When Adding New Documentation:

1. **Project-level docs** → `/README.md`, `/CONTRIBUTING.md`, etc.
2. **Technical guides** → `/docs/` (with appropriate subdirectory)
3. **Status reports** → `/.ai/` (temporary, for tracking)
4. **Test results** → `/artifacts/` (with timestamp)

### When Removing Documentation:

1. Move deprecated docs to `/.ai/archive/` (if needed for history)
2. Delete truly obsolete documents
3. Update references in other docs

---

## Current Organization (After Cleanup)

### Project Root (`/`)
- `README.md` - Main project documentation
- `CONTRIBUTING.md` - How to contribute
- `SECURITY.md` - Security policy
- `LICENCE.md` - MIT License
- `Makefile`, `CMakeLists.txt` - Build files
- `Dockerfile` - Container config

### Documentation (`/docs/`)
- `readiness/MANAGEMENT_DECISION.md` - Deployment decision
- `readiness/SOURCE_OF_TRUTH.md` - Readiness reference
- `validation/EVIDENCE_PACK_SYSTEM.md` - Evidence validation
- `operations/TESTING_GUIDE.md` - Testing guide
- `V2_README.md` - Version 2.0 specific info
- (existing architecture and contract docs)

### Validation Reports (`/.ai/`)
- Working status documents
- Temporary tracking
- Should be consolidated periodically

### Artifacts (`/artifacts/`)
- `sanitizers/` - Sanitizer test results
- `soak/` - Soak test results  
- `router-tests/` - Integration test results

---

## Maintenance Guidelines

### Weekly:
- Review `.ai/` for obsolete docs
- Archive superseded status reports

### Monthly:
- Consolidate validation reports
- Update `CHANGELOG.md` (if applicable)
- Review `docs/` structure

### Before Release:
- Ensure all docs consistent
- Update version-specific docs
- Archive old validation reports

---

## Enforcement

**Automated**:
- CI checks for docs in wrong locations (future)

**Manual**:
- Code review checks documentation placement
- Periodic cleanup (monthly)

---

**Last Updated**: 2025-12-27  
**Version**: 1.0  
**Maintained By**: Project maintainers
