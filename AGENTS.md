## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues using the `gh` CLI. External PRs are not a triage request surface. See `docs/agents/issue-tracker.md`.

### Triage labels

The repo uses the default five-label triage vocabulary. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repo: read root `CONTEXT.md` and root `docs/adr/` when present. See `docs/agents/domain.md`.

## Branch workflow

Use `origin/develop` as the development baseline. Do not develop from an outdated local
branch or rewrite either formal remote branch.

1. Synchronize remote refs before starting work:

   ```sh
   git fetch --prune origin
   git switch develop
   git pull --ff-only origin develop
   ```

2. Create a feature branch from the synchronized local `develop`:

   ```sh
   git switch --create feature/<short-name> develop
   ```

   Keep unrelated untracked files out of commits. Do not create a feature branch from
   `main` or from stale local history.

3. Implement and validate the change on the feature branch. Before merging, review the
   complete feature diff against `develop`, run the relevant tests, and resolve all
   review findings. The feature branch must be clean before it is merged.

4. Merge the reviewed feature into local `develop`:

   ```sh
   git switch develop
   git pull --ff-only origin develop
   git merge --no-ff feature/<short-name>
   ```

   Run the required validation again on the merge result. Push the merge with a normal
   push and wait for any required remote/PR merge or CI completion before proceeding:

   ```sh
   git push origin develop
   ```

5. After the develop merge is complete, update local `main` from its remote branch and
   leave the repository on the current main state:

   ```sh
   git fetch --prune origin
   git switch main
   git pull --ff-only origin main
   ```

Never use force push for `main` or `develop`. If either fast-forward pull fails, stop and
   inspect the remote history instead of resetting or discarding commits.
