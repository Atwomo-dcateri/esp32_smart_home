param(
    [string]$CommitMessage = "",
    [int]$IntervalMinutes = 0,
    [int]$MaxCommits = 0,
    [string]$LogFile = "git-auto-commit.log",
    [switch]$Help
)

function Write-Log {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] $Message"
    Write-Host $logEntry
    Add-Content -Path $LogFile -Value $logEntry
}

function Test-GitStatus {
    try {
        $status = git status --porcelain 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Log "ERROR: Not a git repository or git command failed"
            return $false
        }
        return $true
    }
    catch {
        Write-Log "ERROR: Git command not found"
        return $false
    }
}

function Get-GitBranch {
    try {
        $branch = git rev-parse --abbrev-ref HEAD 2>&1
        return $branch
    }
    catch {
        return "unknown"
    }
}

function Invoke-GitCommit {
    param([string]$Message)
    
    if (-not (Test-GitStatus)) {
        return $false
    }
    
    $branch = Get-GitBranch
    Write-Log "Current branch: $branch"
    
    $status = git status --porcelain
    if ($status.Count -eq 0) {
        Write-Log "No changes to commit"
        return $true
    }
    
    Write-Log "Adding all changes..."
    git add . 2>&1 | Out-Null
    
    if ($LASTEXITCODE -ne 0) {
        Write-Log "ERROR: Failed to add changes"
        return $false
    }
    
    if ([string]::IsNullOrWhiteSpace($Message)) {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        $Message = "Auto commit at $timestamp"
    }
    
    Write-Log "Committing with message: $Message"
    git commit -m $Message 2>&1 | Out-Null
    
    if ($LASTEXITCODE -ne 0) {
        Write-Log "ERROR: Failed to commit changes"
        return $false
    }
    
    Write-Log "Pushing to remote..."
    git push origin $branch 2>&1 | Out-Null
    
    if ($LASTEXITCODE -ne 0) {
        Write-Log "ERROR: Failed to push to remote"
        return $false
    }
    
    Write-Log "SUCCESS: Changes committed and pushed successfully"
    return $true
}

function Show-Help {
    Write-Host @"
Git Auto Commit and Push Script
================================

Usage: .\git-auto-commit.ps1 [options]

Options:
    -CommitMessage <string>     Custom commit message (default: auto-generated timestamp)
    -IntervalMinutes <int>      Run every N minutes (0 = run once, default: 0)
    -MaxCommits <int>           Maximum number of commits (0 = unlimited, default: 0)
    -LogFile <string>           Log file path (default: git-auto-commit.log)
    -Help                       Show this help message

Examples:
    # Single commit with auto-generated message
    .\git-auto-commit.ps1

    # Single commit with custom message
    .\git-auto-commit.ps1 -CommitMessage "Update documentation"

    # Run every 30 minutes with unlimited commits
    .\git-auto-commit.ps1 -IntervalMinutes 30

    # Run every 15 minutes, max 10 commits
    .\git-auto-commit.ps1 -IntervalMinutes 15 -MaxCommits 10

    # Custom log file location
    .\git-auto-commit.ps1 -LogFile "C:\logs\git-commits.log"

Note:
    - This script will add all changes (git add .)
    - Make sure you have push permissions to the remote repository
    - Press Ctrl+C to stop the script when running in interval mode
"@
}

if ($Help) {
    Show-Help
    exit 0
}

Write-Log "========================================"
Write-Log "Git Auto Commit Script Started"
Write-Log "========================================"

if ($IntervalMinutes -gt 0) {
    Write-Log "Running in interval mode: every $IntervalMinutes minutes"
    if ($MaxCommits -gt 0) {
        Write-Log "Maximum commits: $MaxCommits"
    } else {
        Write-Log "Maximum commits: unlimited"
    }
    
    $commitCount = 0
    while ($true) {
        $commitCount++
        Write-Log "--- Commit #$commitCount ---"
        
        $success = Invoke-GitCommit -Message $CommitMessage
        
        if ($MaxCommits -gt 0 -and $commitCount -ge $MaxCommits) {
            Write-Log "Reached maximum commits limit ($MaxCommits)"
            break
        }
        
        Write-Log "Waiting $IntervalMinutes minutes until next commit..."
        Start-Sleep -Seconds ($IntervalMinutes * 60)
    }
} else {
    Write-Log "Running in single commit mode"
    Invoke-GitCommit -Message $CommitMessage | Out-Null
}

Write-Log "========================================"
Write-Log "Git Auto Commit Script Finished"
Write-Log "========================================"
