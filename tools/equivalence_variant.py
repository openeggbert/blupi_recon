#!/usr/bin/env python3
"""Variant of the target-local certificate checker, for SB1 targets the pinned checker cannot bind.

tools/equivalence_target.py is pinned by hash (checker_sha256) in every existing certificate, so it stays
unchanged. This module reuses its proof functions (structure, cfg_structure, sb1_structure, the relocation
resolver and every lemma) without modification. It runs only for targets whose targets.json entry says
"equivalence_checker": "equivalence_variant", and differs from equivalence_target.check in exactly two points:

1. SB1 getter identity. equivalence_target binds ?GetTrueColorBack@CPixmap@@QAEHXZ to the raw-identical STRICT
   body of ?GetEnable@CSound@@QAEHXZ and requires the getter to exist. An earlier source revision whose pixmap
   class has no true-colour field (the June 1998 demo, docs/SB1_DEMO.md) defines no such method. Here the binding
   is made exactly as before when the getter is defined, and skipped only when no object defines it AND no
   relocation in any object names it: then no proof can depend on it.
2. Unequal extents. equivalence_target requires the candidate body and the original function to have the same
   extent. structure() already maps the two ends of the bodies onto each other, so a declaration here may give
   "candidate_size" (the candidate's extent) next to "size" (the original's, from the oracle inventory). It must
   be declared exactly when the two differ, and only for the general region proof (no proof_method or
   cfg-constants). Coverage still counts the original's "size".

Each certificate records checker_sha256 of equivalence_target.py (unchanged) and variant_checker_sha256 of this
file, plus the variant points that applied. EQUIVALENT never counts as STRICT (docs/VERIFYING.md).
"""
from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path

import capstone as cs
import cmpfunc
import pefile
import targets
import equivalence_target as base
from equivalence_target import require, digest, hashed, relocation_binding

ROOT = Path(__file__).resolve().parents[1]
GETTER, GETTER_OWNER = '?GetTrueColorBack@CPixmap@@QAEHXZ', '?GetEnable@CSound@@QAEHXZ'
NAME = 'equivalence_variant'


def getter_absent(paths_):
    """True when no object defines the SB1 getter and no relocation names it."""
    for path in paths_:
        coff = cmpfunc.Coff(path)
        if any(f.name == GETTER for f in coff.functions()):
            return False
        for f in coff.functions():
            if any(coff.sym_by_index[r.symbol_index].name == GETTER for r in f.relocs):
                return False
    return True


def bindings(target, manifest, comparison, evidence, pe, out, library):
    """equivalence_target.bindings, with point 1 of the module docstring. Otherwise line for line the same."""
    tx = next(s for s in pe.sections if s.Name.startswith(b'.text'))
    td, tr, ib = tx.get_data(), tx.VirtualAddress, pe.OPTIONAL_HEADER.ImageBase
    paths_ = [Path(u['object']).resolve() for u in comparison] + list(evidence)
    at = {a['symbol']: int(str(a['rva']), 0) for a in manifest.get('accepted_functions', [])}
    at.update({a['symbol']: int(str(a['rva']), 0) for a in manifest.get('forced_locations', [])})
    explicit = {a['symbol']: int(str(a['va']), 0) for a in manifest.get('symbol_mappings', [])}
    with base.inventory(target):
        shared, global_, fd, dd, _, _, _, _ = cmpfunc.collect_global_state(paths_, pe, td, tr, at, explicit)
        starts = {ib+r for r in cmpfunc.oracle_function_starts()}
    for unit in comparison:
        for row in unit['functions']:
            if row['status'] in ('STRICT', 'FOLDED') and len(row['matches']) == 1:
                key, va = 'global:'+row['name'], ib+int(row['matches'][0]['rva'], 16)
                require(key not in global_ or global_[key] == va, 'conflicting independently checked binding')
                global_[key] = va
    getter = 'not applicable'
    if target.name.startswith('sb1/'):
        if getter_absent(paths_):
            getter = 'absent: no object defines or references ' + GETTER
        else:
            candidates = [(cmpfunc.Coff(path),f) for path in paths_ for f in cmpfunc.Coff(path).functions() if f.name == GETTER]
            require(len(candidates) == 1 and 'global:'+GETTER_OWNER in global_, 'missing SB1 getter identity')
            _, f = candidates[0]
            va = global_['global:'+GETTER_OWNER]
            require(not f.relocs and len(f.data) == 16 and f.data == pe.get_data(va-ib,16),
                    'SB1 getter is not raw-identical to STRICT owner')
            require(global_.get('global:'+GETTER) in (None,va), 'SB1 getter identity conflict')
            global_['global:'+GETTER] = va
            getter = 'bound to the STRICT ' + GETTER_OWNER + ' body, as in equivalence_target'
    needed = {r['symbol'] for r in base.declarations(target, manifest)}
    missing = set()
    for path in paths_:
        coff = cmpfunc.Coff(path)
        for f in coff.functions():
            if f.name in needed:
                for r in f.relocs:
                    name = coff.sym_by_index[r.symbol_index].name
                    if 'global:'+name not in global_:
                        missing.add(name)
    crt = {}
    imports = cmpfunc.import_map(pe)
    for name in sorted(missing):
        if name.startswith('__imp_') and cmpfunc.normalize_import(name) in imports:
            global_['global:'+name] = imports[cmpfunc.normalize_import(name)]
    if missing:
        for coff, functions in base.library_functions(library, out):
            for f in functions:
                if f.name not in missing:
                    continue
                hits = cmpfunc.candidates_for(f, td)
                if f.relocs:
                    if f.name not in ('__findfirst','__findnext'):
                        continue
                    require(len(f.data) == {'__findfirst':304,'__findnext':288}[f.name],
                            'file-enumeration CRT signature extent changed')
                    require(len(hits) == 1, 'file-enumeration CRT signature is not unique')
                    va = ib+tr+hits[0]
                    require(va in starts and f.name not in crt, 'ambiguous CRT signature entry')
                    global_['global:'+f.name] = va
                    crt[f.name] = dict(va=hex(va), size=len(f.data),
                        library_payload_sha256=hashed(dict(bytes=f.data.hex(),relocations=[
                            [r.offset-f.start,r.typ,coff.sym_by_index[r.symbol_index].name] for r in f.relocs])),
                        oracle_body_sha256=hashlib.sha256(td[hits[0]:hits[0]+len(f.data)]).hexdigest(),
                        proof='unique complete relocation-masked pinned-LIBC signature; target identity only, NOT STRICT or CRT behavioral equivalence')
                elif len(hits) == 1 and f.data == td[hits[0]:hits[0]+len(f.data)]:
                    require(f.name not in crt, 'ambiguous CRT library definition')
                    va = ib+tr+hits[0]
                    require(va in starts, 'CRT signature not at an oracle function entry')
                    global_['global:'+f.name] = va
                    crt[f.name] = dict(va=hex(va), size=len(f.data), body_sha256=hashlib.sha256(f.data).hexdigest(),
                                       proof='unique complete raw-identical relocation-free CRT library body')
    return (shared, global_, fd, dd, cmpfunc.import_map(pe), explicit, starts), crt, getter


def extents(entry, row):
    """Point 2 of the module docstring: (original size, candidate size), each checked against its source."""
    size = entry['size']
    csize = entry.get('candidate_size', size)
    require(row['code_size'] == csize, f"{entry['name']}: function extent changed")
    if 'candidate_size' in entry:
        require(csize != size, f"{entry['name']}: candidate_size declared for equal extents")
        require(entry.get('proof_method') in (None, 'cfg-constants'),
                f"{entry['name']}: unequal extents need the general region proof")
    return size, csize


def check(target, manifest, comparison, evidence_objects, out_dir, write=False):
    entries = base.declarations(target, manifest)
    if not entries:
        return {}
    require(__debug__, 'equivalence requires assertions enabled (no -O)')
    require(targets.registry()['targets'][target.name].get('equivalence_checker') == NAME,
            'variant checker runs only for targets that declare it')
    require(target.name.startswith('sb1/') and not target.primary and not target.name.startswith('sb1/eggbert/'),
            'variant checker: SB1 secondary targets other than the Eggbert editions only')
    target.check_exe()
    out = Path(out_dir).resolve()
    require(any(out.is_relative_to(ROOT/d) for d in ('build', 'scratch')), 'proof output must be inside build/ or scratch/')
    out.mkdir(parents=True, exist_ok=True)
    import msvc
    tree = msvc.resolve_tree(manifest['toolchain']['tree'])
    _, vc, _, _ = msvc.tool_layout(tree, None)
    library = vc/'LIB/LIBC.LIB'
    pe = pefile.PE(str(target.exe))
    tx = next(s for s in pe.sections if s.Name.startswith(b'.text'))
    state, crt, getter = bindings(target, manifest, comparison, evidence_objects, pe, out, library)
    results = {}
    certified_entries = {}
    require(len({e['symbol'] for e in entries}) == len(entries), 'duplicate equivalence declarations')
    for entry in entries:
        name, symbol = entry['name'], entry['symbol']
        matches = [(u, f) for u in comparison for f in u['functions'] if f['name'] == symbol]
        require(len(matches) == 1, f'{name}: expected exactly one emitted body')
        unit, row = matches[0]
        rva = int(str(entry['rva']),0)
        size, csize = extents(entry, row)
        require(Path(unit['object']).stem.endswith('_'+Path(entry['tu']).stem), f'{name}: TU changed')
        require(pe.OPTIONAL_HEADER.ImageBase+rva in state[-1], f'{name}: not an oracle entry')
        require(row['status'] in ('MISMATCH', 'STRICT', 'FOLDED'), f'{name}: unexpected byte status')
        if row['status'] in ('STRICT','FOLDED'):
            require(len(row['matches']) == 1 and int(row['matches'][0]['rva'],16) == rva, f'{name}: strict location changed')
            results[name] = dict(status=row['status'], certificate_required=False)
            continue
        coff = cmpfunc.Coff(Path(unit['object']))
        f, = [f for f in coff.functions() if f.name == symbol]
        raw = pe.get_data(rva, size)
        require(len(f.data) == csize and len(raw) == size, f'{name}: incomplete payload')
        regions = [tuple(int(str(x),0) for x in r) for r in entry['regions']]
        pure = base.pure_sort_callee(comparison, pe, entry['pure_callee']) if entry.get('pure_callee') else None
        if entry.get('proof_method') == 'sb1-local':
            sites, checks = base.sb1_structure(f,raw,coff,entry,rva,pe.OPTIONAL_HEADER.ImageBase)
        elif entry.get('proof_method') == 'cfg-constants':
            sites,checks = base.cfg_structure(f,raw,regions,entry['returns_value'])
        else:
            require('proof_method' not in entry, 'unknown proof method')
            sites, checks = base.structure(f, raw, regions, entry['returns_value'], coff, entry.get('pure_callee'),
                entry.get('constant_invariants', False),
                [tuple(int(str(x),0) for x in pair) for pair in entry.get('region_relocations',[])])
        if pure:
            checks['pure_callee_identity'] = pure
        shared, globals_, fd, dd, imports, explicit, starts = state
        rr = []
        for reloc in f.relocs:
            offset = reloc.offset-f.start
            one = replace(f, relocs=[reloc])
            site_explicit = explicit
            if coff.sym_by_index[reloc.symbol_index].name == f.name and sites[offset] != offset:
                self_va = pe.OPTIONAL_HEADER.ImageBase+rva
                require(globals_.get('global:'+f.name) in (None,self_va), 'recursive entry binding conflict')
                site_explicit = dict(explicit, **{f.name:self_va})
            status, rows = cmpfunc.resolve_sites(one, rva-tx.VirtualAddress+sites[offset]-offset,
                tx.get_data(), tx.VirtualAddress, pe.OPTIONAL_HEADER.ImageBase, pe, coff, Path(unit['object']),
                shared, fd, dd, imports, site_explicit, globals_, starts)
            require(status == 'STRICT' and all(r['verified'] for r in rows), f'{name}: unresolved relocation {rows}')
            rr.extend(dict(r, oracle_offset=sites[offset]) for r in rows)
        checks['verified_relocations'] = len(rr)
        checks['relocations_sha256'] = hashed(relocation_binding(rr))
        (out/(name+'-relocations.json')).write_text(json.dumps(rr, indent=2)+'\n', encoding='utf-8')
        dependencies = {coff.sym_by_index[r.symbol_index].name:certified_entries[coff.sym_by_index[r.symbol_index].name]
                        for r in f.relocs if coff.sym_by_index[r.symbol_index].name in certified_entries}
        if dependencies:
            checks['previously_proved_equivalent_callee_entries'] = dependencies
        variant = dict(sb1_getter_identity=getter)
        if csize != size:
            variant['extents'] = dict(original=size, candidate=csize,
                                      method='structure() maps the end of the candidate body onto the end of the original')
        current = dict(schema=2, status='EQUIVALENT', target=target.name, declaration=entry,
                       scope='static-local-equivalence', assumptions=base.ASSUMPTIONS,
                       payload_sha256=hashed(dict(bytes=f.data.hex(), relocations=[
                           [r.offset-f.start,r.typ,coff.sym_by_index[r.symbol_index].name] for r in f.relocs])),
                       oracle_sha256=target.sha256, oracle_function_sha256=hashlib.sha256(raw).hexdigest(),
                       checker_sha256=digest(base.__file__), variant_checker_sha256=digest(__file__), variant=variant,
                       comparator_sha256=digest(ROOT/'tools/cmpfunc.py'),
                       comparator_wrapper_sha256=digest(ROOT/'tools/cmpfunc_target.py'),
                       capstone_version=cs.__version__, crt_library_sha256=digest(library),
                       inventory_sha256={p:digest((target.oracle_dir or ROOT/'oracle')/p) for p in ('functions.csv','tu_map.csv')},
                       crt_target_identity={k:v for k,v in crt.items() if k not in ('__findfirst','__findnext')
                           or any(coff.sym_by_index[r.symbol_index].name == k for r in f.relocs)}, checks=checks)
        current['assumptions'] = [
            'Functions are entered at their declared ABI entry; calls return normally and obey the x86 ABI with flat DS/ES and clear DF. Scratch registers and arithmetic flags are not hidden call arguments or results.',
            'Memory accesses are defined ordinary RAM with no MMIO, concurrency, asynchronous observation or self-modifying code; function code and switch tables remain immutable and are not aliased by pointed-to objects or buffers.',
            'The active stack frame and argument stack do not alias pointed-to objects; dead stack slots below ESP are unobservable. Ordinary loads retain the preceding store trace, without assuming distinct objects do not alias.',
            'Equivalence covers defined memory effects, branch outcomes, identical call targets/arguments and ABI returns; timing, faults on invalid memory and intermediate scratch-register/flags observation are excluded.',
            'Relocation evidence proves target identities, not behavioral equivalence of unmatched callees. File-enumeration CRT identities use unique complete signatures in the pinned library.',
        ]
        path = (ROOT/entry['certificate']).resolve()
        require(path.parent == (ROOT/'evidence/equivalence'/target.slug).resolve() and path.suffix == '.json',
                'certificate outside target evidence directory')
        if write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(current, indent=2)+'\n', encoding='utf-8')
        else:
            base.validate_certificate(json.loads(path.read_text(encoding='utf-8')), current, name)
        results[name] = dict(status='EQUIVALENT', byte_status=row['status'], size=size, rva=hex(rva),
                             certificate=entry['certificate'])
        key,va = 'global:'+f.name,pe.OPTIONAL_HEADER.ImageBase+rva
        if key not in globals_:
            globals_[key] = va
        else:
            require(globals_[key] == va,'certified callee entry conflicts with run-wide identity')
        certified_entries[f.name] = dict(rva=hex(rva),certificate=entry['certificate'],
                                         payload_sha256=current['payload_sha256'])
        print(f'{name}: EQUIVALENT ({size:,} bytes; static local proof, variant checker; byte status MISMATCH)')
    accepted = [r for r in results.values() if r['status'] == 'EQUIVALENT']
    print(f'EQUIVALENT tier: {len(accepted)} functions, {sum(r["size"] for r in accepted):,} bytes (excluded from STRICT coverage)')
    (out/'equivalence.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    return results


def checker_for(target):
    """The certificate checker module a secondary target declares (default: equivalence_target)."""
    if targets.registry()['targets'].get(target.name, {}).get('equivalence_checker') == NAME:
        import equivalence_variant
        return equivalence_variant
    return base


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    targets.add_argument(parser)
    parser.add_argument('--objects-dir', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, default=ROOT/'build/w-equiv-audit')
    parser.add_argument('--write-certificates', action='store_true')
    args = parser.parse_args()
    target = targets.load(args.target)
    require(checker_for(target).__name__ == NAME, 'target does not declare the variant checker')
    import verify
    verify.TARGET, verify.BUILD = target, args.out_dir.resolve()
    verify.GAME_TEXT_END = target.game_text_end
    verify.GAME_TEXT_SIZE = target.game_text_end - verify.ORACLE_TEXT_START
    verify.BUILD.mkdir(parents=True, exist_ok=True)
    objects = sorted(args.objects_dir.resolve().glob('*.obj'))
    evidence = [p for p in objects if p.name.startswith('crt_')]
    comparison = verify.run_comparator([p for p in objects if p not in evidence], evidence, target.manifest())
    by, _ = verify.report(comparison, [u['source'] for u in target.manifest()['translation_units']])
    for index, unit in enumerate(target.manifest()['translation_units']):
        if unit.get('build_path'):
            log = args.objects_dir.parent/'logs'/f'{index:02d}_{Path(unit["source"]).stem}.log'
            command = log.read_text(encoding='utf-8') if log.is_file() else ''
            if unit['build_path'].lower() not in command.lower():
                verify.DEGRADED[unit['source']] = 'historical source path not attested in reused object log'
                print(f'gate INCOMPLETE: {unit["source"]}: {verify.DEGRADED[unit["source"]]}')
    require(not verify.accepted_regressions(target.manifest(), by), 'strict peer regression')
    check(target, target.manifest(), comparison, evidence, verify.BUILD, args.write_certificates)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
