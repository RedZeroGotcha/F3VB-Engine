<#
  F3VB - roda toda a analise e junta os relatorios em .txt (PowerShell 5.1 ou 7+).

  Uso (na pasta do projeto):
    powershell -ExecutionPolicy Bypass -File .\scripts\rodar_tudo.ps1 -PastaDemo "C:\caminho\da\demo"
  Com amostras binarias (gr2/b3d/tga/amx) e analise profunda dos arquivos:
    powershell -ExecutionPolicy Bypass -File .\scripts\rodar_tudo.ps1 -PastaDemo "C:\caminho\da\demo" -IncluirAmostras

  Saidas (tudo em private\, que o git ignora):
    private\relatorio\*.txt           relatorios de texto
    private\ENVIAR_PARA_ANALISE.zip   zip com os .txt (e, com -IncluirAmostras, os binarios das amostras)
#>
param(
    [Parameter(Mandatory = $true)] [string] $PastaDemo,
    [switch] $IncluirAmostras,
    [int] $PorClasse = 2
)

$ErrorActionPreference = 'Continue'
$Raiz     = Split-Path -Parent $PSScriptRoot
$PyDir    = Join-Path $Raiz 'tools\py'
$Saida    = Join-Path $Raiz 'private\relatorio'
$Amostra  = Join-Path $Raiz 'private\amostra'
$ZipFinal = Join-Path $Raiz 'private\ENVIAR_PARA_ANALISE.zip'

# Acentos corretos nos .txt: o PowerShell precisa ler a saida do Python como UTF-8
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false
$OutputEncoding = New-Object System.Text.UTF8Encoding $false

function Passo([string] $Texto) { Write-Host ''; Write-Host "==> $Texto" -ForegroundColor Cyan }
function Nome-Seguro([string] $Texto) { return ($Texto -replace '[^A-Za-z0-9_.-]', '_') }

# ---- 1) achar o Python (3.10+) ------------------------------------------------
$PyExe = $null
$PyArgs = @()
foreach ($tentativa in @('py', 'python', 'python3')) {
    $cmd = Get-Command $tentativa -ErrorAction SilentlyContinue
    if ($null -eq $cmd) { continue }
    $extra = @()
    if ($tentativa -eq 'py') { $extra = @('-3') }
    $versao = & $cmd.Source @extra --version 2>&1
    if (($LASTEXITCODE -eq 0) -and ("$versao" -match 'Python 3\.(\d+)') -and ([int]$Matches[1] -ge 10)) {
        $PyExe = $cmd.Source
        $PyArgs = $extra
        Write-Host "Python encontrado: $versao ($PyExe)"
        break
    }
}
if ($null -eq $PyExe) {
    Write-Host 'Python 3.10+ nao encontrado. Instale com:  winget install Python.Python.3.12' -ForegroundColor Red
    Write-Host 'Depois feche e abra o PowerShell de novo e rode este script outra vez.'
    exit 1
}
if (-not (Test-Path -LiteralPath $PastaDemo -PathType Container)) {
    Write-Host "Pasta da demo nao encontrada: $PastaDemo" -ForegroundColor Red
    exit 1
}
$PastaDemo = (Resolve-Path -LiteralPath $PastaDemo).Path

New-Item -ItemType Directory -Force -Path $Saida | Out-Null
New-Item -ItemType Directory -Force -Path $Amostra | Out-Null
$env:PYTHONUTF8 = '1'
$env:F3VB_GAME_DIR = $PastaDemo

# Roda o Python com os argumentos dados e grava a saida (stdout+stderr) num .txt. Devolve o codigo de saida.
function Invoke-Py([string] $Arquivo, [string[]] $Argumentos) {
    $todos = @() + $PyArgs + $Argumentos
    $linhas = & $PyExe @todos 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -ne 'System.Management.Automation.RemoteException' }
    $codigo = $LASTEXITCODE
    $linhas | Out-File -FilePath $Arquivo -Encoding utf8
    return $codigo
}
function Invoke-F3([string] $Arquivo, [string[]] $Argumentos) {
    return (Invoke-Py $Arquivo (@('-m', 'f3vb') + $Argumentos))
}

Push-Location $PyDir
try {
    # ---- 2) testes automaticos (confirma que o ambiente esta ok) ----------------
    Passo 'Testes automaticos'
    $c = Invoke-Py (Join-Path $Saida '00_testes.txt') @('-m', 'unittest', 'discover', '-s', 'tests', '-t', '.')
    Write-Host "   codigo de saida: $c (0 = tudo certo)"

    # ---- 3) inventario da pasta ---------------------------------------------------
    Passo 'Inventario da pasta da demo'
    $c = Invoke-F3 (Join-Path $Saida '01_inventario.txt') @('inventory', $PastaDemo)
    Write-Host "   codigo: $c"

    # ---- 4) resource.rht ------------------------------------------------------------
    Passo 'resource.rht'
    $rht = Get-ChildItem -LiteralPath $PastaDemo -Recurse -Filter 'resource.rht' -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $rht) {
        $c = Invoke-F3 (Join-Path $Saida '02_rht.txt') @('rht', $rht.FullName, '--entries', '60')
        Write-Host "   $($rht.FullName)  codigo: $c"
    } else {
        'resource.rht NAO encontrado na pasta (procure por outro nome/extensao .rht).' | Out-File (Join-Path $Saida '02_rht.txt') -Encoding utf8
        Write-Host '   resource.rht nao encontrado' -ForegroundColor Yellow
    }

    # ---- 5) catalogo: cruza o .rht com os .grp (nomes e tipos de cada recurso) -------------
    Passo 'Catalogo de recursos (rht x grp)'
    $c = Invoke-F3 (Join-Path $Saida '02b_catalogo_log.txt') @('catalog', $PastaDemo, '--out', (Join-Path $Saida 'catalogo'))
    Write-Host "   codigo: $c"

    # ---- 6) relatorio geral + resumo curto ----------------------------------------------
    Passo 'Relatorio geral (le todos os .grp; pode demorar alguns minutos)'
    $c = Invoke-F3 (Join-Path $Saida '03_log_report.txt') @('report', $PastaDemo, '-o', (Join-Path $Saida '03_report.json'), '--resumo', (Join-Path $Saida '03_resumo.txt'))
    Write-Host "   codigo: $c"

    # ---- 7) lista de lumps de cada .grp (ignora copias identicas: mesmo nome e tamanho) ------
    $todosGrps = @(Get-ChildItem -LiteralPath $PastaDemo -Recurse -Filter '*.grp' -File -ErrorAction SilentlyContinue | Sort-Object { $_.FullName.Length })
    $vistos = @{}
    $grps = @()
    foreach ($g in $todosGrps) {
        $k = "$($g.Name.ToLower())|$($g.Length)"
        if ($vistos.ContainsKey($k)) {
            Write-Host "   duplicata ignorada: $($g.FullName)" -ForegroundColor Yellow
            continue
        }
        $vistos[$k] = $true
        $grps += $g
    }
    Passo "Lumps de cada .grp ($($grps.Count) arquivo(s) unicos)"
    foreach ($g in $grps) {
        $rel = $g.FullName.Substring($PastaDemo.Length).TrimStart('\', '/')
        $n = Nome-Seguro ([System.IO.Path]::ChangeExtension($rel, $null))
        $c = Invoke-F3 (Join-Path $Saida "04_grp_$n.txt") @('grp', $g.FullName, '--list')
        Write-Host "   $rel  codigo: $c"
    }

    # ---- 8) amostras ------------------------------------------------------------------------------------
    Passo 'Amostras (gr2, b3d, tga, amx) de cada .grp'
    $dirsAmostra = @()
    foreach ($g in $grps) {
        $rel = $g.FullName.Substring($PastaDemo.Length).TrimStart('\', '/')
        $n = Nome-Seguro ([System.IO.Path]::ChangeExtension($rel, $null))
        $dest = Join-Path $Amostra $n
        $args2 = @('sample', $g.FullName, '--out', $dest, '--classes', 'gr2,b3d,tga,amx', '--per-class', "$PorClasse")
        if (-not $IncluirAmostras) { $args2 += '--so-texto' }
        $c = Invoke-F3 (Join-Path $Saida "05_sample_$n.txt") $args2
        Write-Host "   $rel  codigo: $c"
        $dirsAmostra += $dest
    }

    # ---- 8b) falhas do B3D em texto (hexdump do menor arquivo de cada motivo): permite decifrar sem enviar binarios ----
    Passo 'Falhas do B3D (texto)'
    $rhtArg = @()
    if ($null -ne $rht) { $rhtArg = @('--rht', $rht.FullName) }
    foreach ($g in $grps) {
        if (@('Tiles', 'Critters', 'Props', 'Items', 'Effects') -notcontains $g.BaseName) { continue }
        $n = Nome-Seguro $g.BaseName
        $c = Invoke-F3 (Join-Path $Saida "07_b3d_falhas_$n.txt") (@('b3dfail', $g.FullName, '--per-reason', '2') + $rhtArg)
        Write-Host "   $($g.Name)  codigo: $c"
    }

    # ---- 9) analise profunda dos arquivos amostrados (so com -IncluirAmostras) -----------------------------
    if ($IncluirAmostras) {
        Passo 'Analise profunda dos arquivos amostrados'
        foreach ($d in $dirsAmostra) {
            if (-not (Test-Path -LiteralPath $d)) { continue }
            $pasta = Nome-Seguro (Split-Path $d -Leaf)
            foreach ($f in (Get-ChildItem -LiteralPath $d -File)) {
                $n = Nome-Seguro $f.BaseName
                if ($f.Extension -eq '.gr2') {
                    $c = Invoke-F3 (Join-Path $Saida "06_gr2_${pasta}_$n.txt") @('gr2', $f.FullName)
                    Write-Host "   gr2 $($f.Name) codigo: $c"
                } elseif (@('.b3d', '.tga', '.amx', '.bmp') -contains $f.Extension) {
                    $c = Invoke-F3 (Join-Path $Saida "06_inspect_${pasta}_$n.txt") @('inspect', $f.FullName)
                    Write-Host "   inspect $($f.Name) codigo: $c"
                }
            }
        }
    }

    # ---- 10) juntar tudo num zip (feito pelo Python: barras normais nos nomes) --------------------------------
    Passo 'Montando o zip para analise'
    $destAm = Join-Path $Saida 'amostras'
    New-Item -ItemType Directory -Force -Path $destAm | Out-Null
    Get-ChildItem -LiteralPath $Amostra -Filter '*.zip' -File -ErrorAction SilentlyContinue | Copy-Item -Destination $destAm -Force
    if (Test-Path -LiteralPath $ZipFinal) { Remove-Item -LiteralPath $ZipFinal -Force }
    $c = Invoke-Py (Join-Path $Saida '99_zip_log.txt') @('-m', 'f3vb', 'zipdir', $Saida, $ZipFinal)
    $mb = [math]::Round((Get-Item -LiteralPath $ZipFinal).Length / 1MB, 2)
    Write-Host ''
    Write-Host "PRONTO. Envie: $ZipFinal  ($mb MB)" -ForegroundColor Green
    Write-Host "Se for grande, envie so: $(Join-Path $Saida '03_resumo.txt') e $(Join-Path $Saida 'catalogo\catalogo_resumo.txt')"
}
finally {
    Pop-Location
}
