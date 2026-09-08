// PROSPECT / BSM 光谱模型（与 HiStream compo.cpp 一致的移植）
// 系数来自 C:\work\streamsim\models\histream\defined\optipar.txt

export function parseOptipar(text) {
  const columns = []
  for (const line of String(text).split(/\r?\n/)) {
    const trimmed = line.trim()
    if (!trimmed) continue
    const fields = trimmed.split(/\s+/).map(Number)
    if (fields.length < 18 || fields.some((v) => !Number.isFinite(v))) continue
    columns.push(fields)
  }
  return {
    wl: columns.map((row) => row[0]),
    nr: columns.map((row) => row[1]),
    kab: columns.map((row) => row[2]),
    ks: columns.map((row) => row[4]),
    kw: columns.map((row) => row[5]),
    kdm: columns.map((row) => row[6]),
    phiI: columns.map((row) => row[7]),
    phiII: columns.map((row) => row[8]),
    gsv1: columns.map((row) => row[14]),
    gsv2: columns.map((row) => row[15]),
    gsv3: columns.map((row) => row[16]),
    nw: columns.map((row) => row[17])
  }
}

function calctav(alfa, nr) {
  const pi = 3.1415926
  const rd = pi / 180
  const n2 = nr * nr
  const np = n2 + 1
  const nm = n2 - 1
  const a = (nr + 1) * (nr + 1) / 2
  const k = -(n2 - 1) * (n2 - 1) / 4
  const sa = Math.sin(alfa * rd)
  let b1 = 0
  if (alfa !== 90) b1 = Math.sqrt((sa * sa - np / 2) * (sa * sa - np / 2) + k)
  const b2 = sa * sa - np / 2
  const b = b1 - b2
  const b3 = b * b * b
  const a3 = a * a * a
  const ts = (k * k / (6 * b3) + k / b - b / 2) - (k * k / (6 * a3) + k / a - a / 2)
  const tp1 = -2 * n2 * (b - a) / (np * np)
  const tp2 = -2 * n2 * np * Math.log(b / a) / (nm * nm)
  const tp3 = n2 * (1 / b - 1 / a) / 2
  const tp4 = 16 * n2 * n2 * (n2 * n2 + 1) * Math.log((2 * np * b - nm * nm) / (2 * np * a - nm * nm)) / (np * np * np * nm * nm)
  const tp5 = 16 * n2 * n2 * n2 * (1 / (2 * np * b - nm * nm) - 1 / (2 * np * a - nm * nm)) / (np * np * np)
  return (ts + tp1 + tp2 + tp3 + tp4 + tp5) / (2 * sa * sa)
}

function expint(x) {
  let sum = 0
  for (let i = 1000; i < 100000; i += 1) {
    const ii = i / 1000
    sum += Math.exp(-x * ii) / ii * 0.001
  }
  return sum
}

function fluspect(coeff, p) {
  const out = []
  for (let i = 0; i < coeff.nr.length; i += 1) {
    const nr = coeff.nr[i]
    const Kdm = coeff.kdm[i]
    const Kab = coeff.kab[i]
    const Kw = coeff.kw[i]
    const Ks = coeff.ks[i]
    const Kall = (p.Cab * Kab + p.Cdm * Kdm + p.Cw * Kw + p.Cs * Ks) / p.N
    const t1 = (1 - Kall) * Math.exp(-Kall)
    const t2 = Kall * Kall * expint(Kall)
    let tau = 1
    if (Kall > 0) tau = t1 + t2
    const talf = calctav(59, nr)
    const ralf = 1 - talf
    const t12 = calctav(90, nr)
    const r12 = 1 - t12
    const t21 = t12 / (nr * nr)
    const r21 = 1 - t21
    const denom = 1 - r21 * r21 * tau * tau
    const Ta = talf * tau * t21 / denom
    const Ra = ralf + r21 * tau * Ta
    const tt = t12 * tau * t21 / denom
    const r = r12 + r21 * tau * tt
    const D = Math.sqrt((1 + r + tt) * (1 + r - tt) * (1 - r + tt) * (1 - r - tt))
    const rq = r * r
    const tq = tt * tt
    const a = (1 + rq - tq + D) / (2 * r)
    const b = (1 - rq + tq + D) / (2 * tt)
    const bNm1 = Math.pow(b, p.N - 1)
    const bN2g = bNm1 * bNm1
    const a2 = a * a
    const denom2 = a2 * bN2g - 1
    let Rsub = a * (bN2g - 1) / denom2
    let Tsub = bNm1 * (a2 - 1) / denom2
    if (r + tt >= 1) {
      Tsub = tt / (tt + (1 - tt) * (p.N - 1))
      Rsub = 1 - Tsub
    }
    const denom3 = 1 - Rsub * r
    out.push({
      reflectance: Ra + Ta * Rsub * tt / denom3,
      transmittance: Ta * Tsub / denom3
    })
  }
  return out
}

function bsm(coeff, p) {
  const B = p.BSMBrightness
  const lat = p.BSMlat
  const lon = p.BSMlon
  const rawSMC = Number(p.SMC)
  const SMC = Math.max(0, Math.min(1, rawSMC > 1 ? rawSMC / 100 : rawSMC))
  const SMCp = 0.25
  const film = 0.015
  const rd = Math.PI / 180
  const f1 = B * Math.sin(lat * rd)
  const f2 = B * Math.cos(lat * rd) * Math.sin(lon * rd)
  const f3 = B * Math.cos(lat * rd) * Math.cos(lon * rd)
  const out = []
  for (let k = 0; k < coeff.gsv1.length; k += 1) {
    const rdry = coeff.gsv1[k] * f1 + coeff.gsv2[k] * f2 + coeff.gsv3[k] * f3
    const tw = Math.exp(-coeff.kw[k] * film)
    const rbac = 1 - (1 - rdry) * (rdry * calctav(90, 2.0 / coeff.nw[k]) / calctav(90, 2.0) + 1 - rdry)
    const pv = 1 - calctav(90, coeff.nw[k]) / coeff.nw[k] / coeff.nw[k]
    const Rw = 1 - calctav(40, coeff.nw[k])
    const Radd = (1 - Rw) * (1 - pv) * rbac / (1 - pv * rbac)
    const mu = (SMC - 0.05) / SMCp
    const fdry = Math.exp(-mu)
    const fmul = (Math.exp(tw * mu) - 1) * fdry
    out.push({ reflectance: rdry * fdry + Rw * (1 - fdry) + Radd * fmul, transmittance: 0 })
  }
  return out
}

function sampleAtWaves(spectrum, coeff, waves) {
  const selected = (waves || []).map(Number).filter((w) => Number.isFinite(w) && w >= 400 && w <= 2400)
  const bands = selected.length ? selected : [550, 850]
  const reflectance = []
  const transmittance = []
  for (const wave of bands) {
    let best = 0
    let bestDiff = Infinity
    for (let i = 0; i < coeff.wl.length; i += 1) {
      const diff = Math.abs(coeff.wl[i] - wave)
      if (diff < bestDiff) { bestDiff = diff; best = i }
    }
    reflectance.push(Number(spectrum[best].reflectance.toFixed(6)))
    transmittance.push(Number(spectrum[best].transmittance.toFixed(6)))
  }
  return { reflectance: reflectance.join(','), transmittance: transmittance.join(',') }
}

export function computeSpectralValues(model, params, waves, coeff) {
  if (model === 'Prospect') {
    const spectrum = fluspect(coeff, {
      Cab: Number(params.Cab) || 40, Cw: Number(params.Cw) || 0.01,
      Cdm: Number(params.Cdm) || 0.01, Cs: Number(params.Cs) || 0, N: Number(params.N) || 1.5
    })
    return sampleAtWaves(spectrum, coeff, waves)
  }
  if (model === 'BSM') {
    const spectrum = bsm(coeff, {
      SMC: Number(params.SMC) || 25, BSMBrightness: Number(params.BSMBrightness) || 0.5,
      BSMlat: Number.isFinite(Number(params.BSMlat)) ? Number(params.BSMlat) : 25,
      BSMlon: Number.isFinite(Number(params.BSMlon)) ? Number(params.BSMlon) : 45
    })
    return sampleAtWaves(spectrum, coeff, waves)
  }
  return { reflectance: '', transmittance: '' }
}
