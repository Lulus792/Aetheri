"""Host orchestration guards. No guest, installer or physical disk is executed."""
import json
from contextlib import closing
import sqlite3
import stat
from pathlib import Path
import tempfile
import subprocess
import unittest
from unittest.mock import MagicMock, patch

import provision_windows as provision
import run_vm as vm
import test_linux_policy as policy
import p0_services as services
import test_execute_policy as execute
import test_execution_runtime as execution_runtime


class VmTools(unittest.TestCase):
    def test_live_authority_capabilities_are_observed_not_only_configured(self):
        fields=('CapInh','CapPrm','CapEff','CapBnd','CapAmb')
        empty='\n'.join(key+':\t0000000000000000' for key in fields)
        with patch.object(execution_runtime.Path,'read_text',return_value=empty):
            self.assertEqual(set(execution_runtime.require_empty_capabilities(123)),set(fields))
        for key in fields:
            nonempty=empty.replace(key+':\t0000000000000000',key+':\t0000000000000001')
            with patch.object(execution_runtime.Path,'read_text',return_value=nonempty):
                with self.assertRaisesRegex(RuntimeError,'capabilities'): execution_runtime.require_empty_capabilities(123)
        with patch.object(execution_runtime.Path,'read_text',return_value=empty.replace('CapAmb:', 'Missing:')):
            with self.assertRaises(KeyError): execution_runtime.require_empty_capabilities(123)

    def test_user_bootstrap_requires_exact_new_files_and_independent_context(self):
        work=self.root/'bridge'; (work/'registry').mkdir(parents=True); control='users/account/control'; (work/control).mkdir(parents=True)
        profile=bytearray(384)
        for offset,n in ((40,50),(56,51),(72,4),(88,2)): profile[offset:offset+16]=execution_runtime.identity(n)
        (work/'registry/execution.conf').write_bytes(profile)
        path=work/control/'catalogue.sqlite3'
        with closing(sqlite3.connect(path)) as db,db:
            db.execute('CREATE TABLE authority_meta(kind,authority_epoch,owner_scope_ref,runtime_incarnation,current_boot_id,catalogue_revision)')
            db.execute('INSERT INTO authority_meta VALUES(?,?,?,?,?,?)',('user',bytes(profile[40:56]),bytes(profile[88:104]),bytes(profile[56:72]),bytes(profile[72:88]),2))
            for table in ('install_bindings','data_epochs','launches','operations'): db.execute('CREATE TABLE '+table+'(id)')
        context=b'system_u:object_r:aetheri_user_state_t:s0'
        before={control:[10,1000,1000,stat.S_IFDIR|0o700,2,0,1,1,context.hex(),None]}
        database=control+'/catalogue.sqlite3'; lock=control+'/writer.lock'
        for ending in (b'',b'\0'):
            current=json.loads(json.dumps(before))
            for n,name in enumerate((database,lock)):
                current[name]=[20+n,1000,1000,stat.S_IFREG|0o600,1,32,2,2,(context+ending).hex(),'00'*32]
            record=dict(tree=before,user_control_rel=control,user_catalogue_rel=database)
            for kind in ('extra','owner','replaced','context'):
                bad=json.loads(json.dumps(current))
                if kind=='extra': bad[control+'/foreign']=[*bad[lock]]
                elif kind=='owner': bad[database][1]=0
                elif kind=='replaced': bad[control][0]+=1
                else: bad[database][8]=(context+b':c1').hex()
                with patch.object(execution_runtime,'capture',return_value=bad),patch.object(execution_runtime,'save') as saved:
                    with self.assertRaises(RuntimeError): execution_runtime.acknowledge_user_start(record,work)
                    saved.assert_not_called()
            with patch.object(execution_runtime,'capture',return_value=current),patch.object(execution_runtime,'save') as saved:
                with closing(sqlite3.connect(path)) as db,db: db.execute('INSERT INTO operations VALUES(1)')
                with self.assertRaisesRegex(RuntimeError,'unrequested lifecycle'): execution_runtime.acknowledge_user_start(record,work)
                with closing(sqlite3.connect(path)) as db,db: db.execute('DELETE FROM operations')
                with closing(sqlite3.connect(path)) as db,db: db.execute('UPDATE authority_meta SET runtime_incarnation=zeroblob(16)')
                with self.assertRaisesRegex(RuntimeError,'startup context'): execution_runtime.acknowledge_user_start(record,work)
                with closing(sqlite3.connect(path)) as db,db: db.execute('UPDATE authority_meta SET runtime_incarnation=?',(bytes(profile[56:72]),))
                saved.assert_not_called()
                execution_runtime.acknowledge_user_start(record,work)
                saved.assert_called_once_with(record)
                self.assertEqual(record['tree'],current)

    def test_live_probe_keeps_normal_gates_and_holders_quiescent(self):
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            paths={role:Path(directory)/role for role in ('system','user')}
            for role,path in paths.items():
                path.mkdir()
                with closing(sqlite3.connect(path/'catalogue.sqlite3')) as db, db:
                    db.executescript("CREATE TABLE admission_gates(state TEXT); INSERT INTO admission_gates VALUES('closed'); CREATE TABLE launch_dispatches(state TEXT); CREATE TABLE generation_leases(state TEXT);")
            with patch.object(execution_runtime.normal,'verified'), patch.object(execution_runtime.normal,'state_path',side_effect=lambda r,role:paths[role]):
                execution_runtime.quiet_normal({'pending':None})
                for role,table,state in (('user','admission_gates','open'),('system','launch_dispatches','reserved'),('system','generation_leases','running')):
                    with closing(sqlite3.connect(paths[role]/'catalogue.sqlite3')) as db, db: db.execute('INSERT INTO '+table+' VALUES(?)',(state,))
                    with self.assertRaises(RuntimeError): execution_runtime.quiet_normal({'pending':None})
                    with closing(sqlite3.connect(paths[role]/'catalogue.sqlite3')) as db, db: db.execute('DELETE FROM '+table+' WHERE state=?',(state,))
                with self.assertRaises(RuntimeError): execution_runtime.quiet_normal({'pending':'unknown'})

    def test_live_execution_unit_file_uses_the_exact_wire_unit_name(self):
        for name in ('aetheri-p0-execution-root.service','aetheri-app-0000000000004000800000000000001f.service'):
            self.assertEqual(execution_runtime.unit_file(name).name,name)
        for invalid in ('../foreign.service','service','wrong.service.service'):
            with self.assertRaises(ValueError): execution_runtime.unit_file(invalid)

    def test_live_execution_snapshot_persists_binary_labels_and_digests(self):
        captured={'code':(17,0,0,0o100555,1,12,100,200,b'system_u:object_r:aetheri_store_code_t:s0',bytes(range(32))),
                  'data':(18,1000,1000,0o40700,2,0,100,200,b'system_u:object_r:aetheri_app_data_t:s0:c35,c36',None)}
        with patch.object(execution_runtime,'snapshot',return_value=captured): result=execution_runtime.capture(Path('fixture'))
        restored=json.loads(json.dumps(result))
        self.assertEqual(restored,result)
        self.assertEqual(bytes.fromhex(restored['code'][9]),bytes(range(32)))
        self.assertEqual(bytes.fromhex(restored['data'][8]),captured['data'][8])
        self.assertIsNone(restored['data'][9])

    def test_live_execution_bad_record_does_not_create_a_partial_ack_file(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(execution_runtime,'ROOT',Path(directory)):
            with self.assertRaises(TypeError): execution_runtime.save({'unpersistable':b'raw'})
            self.assertEqual(list(Path(directory).iterdir()),[])

    def test_execution_data_owner_validation_uses_frozen_owner_fields(self):
        def identity(n): return f"000000000000400080000000{n:08x}"
        def fixture(n):
            result={key:identity(n+i) for i,key in enumerate(services.FIXTURE_IDS)}
            result.update(scope=identity(2),artifact=f"{n:064x}"); return result
        def owner(n):
            result={key:identity(n+i) for i,key in enumerate(services.runtime_contracts.OWNER_FIELDS.split())}
            result.update(machine=identity(3),volume=identity(4),account=identity(2)); return result
        def data(f,n):
            return dict(owner=owner(n),install=f["install"],publisher=f["publisher"],system_epoch=identity(5),user_epoch=identity(6),install_revision=1,base_revision=1,schema_version=0)
        original,new=fixture(20),fixture(40)
        record=dict(token="00000000-0000-4000-8000-000000000001",owner=identity(2),epochs=dict(system=identity(5),user=identity(6)),
            units=[],pending=None,fixture=original,fixture_publication={},storage=owner(60),app_data=data(original,80),exec_fixture=dict(fixture=new,app_data=data(new,100)))
        services.validate_record(record)
        for field in ("storage","data","allocation_operation"):
            changed=json.loads(json.dumps(record)); changed["exec_fixture"]["app_data"]["owner"][field]=record["app_data"]["owner"][field]
            with self.assertRaisesRegex(RuntimeError,"aliases existing private ownership"): services.validate_record(changed)
        changed=json.loads(json.dumps(record));changed["exec_fixture"]["app_data"]["install"]=record["fixture"]["install"]
        with self.assertRaisesRegex(RuntimeError,"another selected installation"): services.validate_record(changed)

    def test_execution_data_has_independent_leaves_under_enrolled_shared_parents(self):
        record={"owner":"account","app_data":{"owner":{"storage":"old-storage"}},
                "exec_fixture":{"app_data":{"owner":{"storage":"new-storage"}}}}
        old=services.app_data_paths(record); new=services.app_data_paths(record,"exec")
        for key in ("anchor","registry"): self.assertEqual(old[key],new[key])
        for key in ("root","allocation","payload","owner","owner_config","config"): self.assertNotEqual(old[key],new[key])
        self.assertEqual(new["config"].name,"exec-app-data.conf")
        self.assertEqual(old["config"].name,"app-data.conf")
        with self.assertRaises(ValueError): services.app_data_paths(record,"arbitrary")

    def test_execution_data_and_projection_keep_their_scoped_user_roles(self):
        record={"token":"00000000-0000-4000-8000-000000000001","owner":"00000000000040008000000000000002",
                "pending":None,"units":[]}
        with patch.object(services,"save"),patch.object(services,"run",return_value="") as command,patch.object(services,"publish_broker_slot") as broker:
            unit=services.start_unit(record,"user",data="enroll",data_profile="exec")
            self.assertIn("-user-data-exec-enroll-",unit)
            self.assertEqual(command.call_args.args[-1],"--exec-data-enroll")
            self.assertIn("--property=CapabilityBoundingSet=",command.call_args.args)
            broker.assert_not_called()
        with patch.object(services,"save"),patch.object(services,"run",return_value="") as command,patch.object(services,"publish_projection_slot") as selected:
            unit=services.start_unit(record,"user",probe="projection",launch_probe="exec-refresh")
            selected.assert_called_once_with(record,unit)
            self.assertEqual(command.call_args.args[-1],"--exec-refresh")
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            for role,kwargs in (("system",dict(data="enroll")),("user",{}),("user",dict(data="enroll",probe="projection"))):
                with self.assertRaises(ValueError): services.start_unit(record,role,data_profile="exec",**kwargs)

    def test_execution_fixture_cannot_reuse_original_install_or_slot_identity(self):
        def fixture(offset):
            ids={key:f"000000000000400080000000{n+offset:08x}" for n,key in enumerate(services.FIXTURE_IDS,1)}
            ids["scope"]="00000000000040008000000000000002"
            ids["artifact"]=f"{offset:064x}"; return ids
        record={"token":"00000000-0000-4000-8000-000000000001","owner":"00000000000040008000000000000002",
                "epochs":{"system":"00000000000040008000000000000003","user":"00000000000040008000000000000004"},
                "units":[],"pending":None,"fixture":fixture(30),"fixture_publication":{},"exec_fixture":{"fixture":fixture(60)}}
        services.validate_record(record)
        for key in ("plan","operation","request_key","install","generation","slot","artifact"):
            changed=json.loads(json.dumps(record)); changed["exec_fixture"]["fixture"][key]=record["fixture"][key]
            with self.assertRaisesRegex(RuntimeError,"aliases"): services.validate_record(changed)

    def test_execution_fixture_keeps_original_selection_and_paths(self):
        record={"fixture":{"slot":"original-slot","artifact":"original-artifact"},
                "exec_fixture":{"fixture":{"slot":"new-slot","artifact":"new-artifact"}}}
        before=json.dumps(record,sort_keys=True)
        old=services.fixture_paths(record); new=services.fixture_paths(record,"exec")
        self.assertEqual(old["store"],new["store"])
        for name in ("parent","source","staging","container","target","config","manifest","store_config","activation_config"):
            self.assertNotEqual(old[name],new[name])
        self.assertEqual(old["config"].name,"fixture.conf"); self.assertEqual(new["config"].name,"exec-fixture.conf")
        self.assertEqual(json.dumps(record,sort_keys=True),before)
        with self.assertRaises(ValueError): services.fixture_paths(record,"../arbitrary")

    def test_execution_fixture_action_is_offline_system_administration(self):
        record={"token":"00000000-0000-4000-8000-000000000001","owner":"00000000000040008000000000000002",
                "epochs":{"system":"00000000000040008000000000000003","user":"00000000000040008000000000000004"},
                "pending":None,"units":[],"exec_fixture":{"fixture":{"slot":"new-slot","artifact":"new-artifact"}}}
        with patch.object(services,"save"),patch.object(services,"run",return_value="") as command:
            unit=services.start_unit(record,"system",fixture="store-publish",fixture_profile="exec")
            self.assertIn("-system-fixture-exec-store-publish-",unit)
            self.assertEqual(command.call_args.args[-1],"--exec-fixture-store-publish")
            self.assertIn("--service-type=exec",command.call_args.args)
            self.assertIn("--property=CapabilityBoundingSet=",command.call_args.args)
            self.assertIn("--property=ReadOnlyPaths="+str(services.fixture_paths(record,"exec")["source"]),command.call_args.args)
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            for kwargs in (dict(),dict(fixture="stage",role="user"),dict(fixture="stage",admit=True),dict(fixture="arbitrary")):
                role=kwargs.pop("role","system")
                with self.assertRaises(ValueError): services.start_unit(record,role,fixture_profile="exec",**kwargs)

    def test_execute_retirement_preserves_unacknowledged_dispatch(self):
        root=MagicMock(spec=Path); marker=MagicMock(spec=Path)
        root.__truediv__.return_value=marker
        root.is_symlink.return_value=False; marker.is_symlink.return_value=False
        root.lstat.return_value=type("Info",(),dict(st_mode=stat.S_IFDIR|0o700,st_uid=0))()
        marker.lstat.return_value=type("Info",(),dict(st_mode=stat.S_IFREG|0o600,st_uid=0,st_nlink=1))()
        marker.read_text.return_value=json.dumps(dict(pending="unacknowledged-unit"))
        with patch.object(execute,"ROOT",root),patch.object(execute,"run",side_effect=AssertionError("policy mutation")), \
             patch.object(execute,"drained",side_effect=AssertionError("unknown dispatch")):
            with self.assertRaisesRegex(RuntimeError,"causal reconciliation"): execute.retire()
        marker.unlink.assert_not_called(); root.rmdir.assert_not_called()

    def test_execute_drain_requires_independent_process_cgroup_and_job_absence(self):
        job=dict(type="a(usssoo)",data=[[]])
        unit="aetheri-p0-execute-owned.service"
        for state,pid,cgroup,jobs in (("inactive","0","",job),("failed","0","",job),
                ("active","0","",job),("inactive","99","",job),("failed","0","/live",job),
                ("inactive","0","",dict(type="a(usssoo)",data=[[[1,unit,"stop","running","/job","/unit"]]]))):
            facts=f"ActiveState={state}\nMainPID={pid}\nControlGroup={cgroup}"
            with patch.object(execute,"run",side_effect=[facts,json.dumps(jobs)]):
                if state in ("inactive","failed") and pid=="0" and not cgroup and jobs is job:
                    execute.drained(unit)
                else:
                    with self.assertRaisesRegex(RuntimeError,"independently drained"): execute.drained(unit)

    def test_execute_resets_only_observed_drained_failed_units(self):
        with patch.object(execute,"drained",side_effect=["inactive","failed"]) as observed,patch.object(execute,"run") as command:
            execute.reset_drained_failures(["owned-a","owned-b"])
            self.assertEqual(observed.call_count,2)
            command.assert_called_once_with("systemctl","reset-failed","owned-b")
        with patch.object(execute,"drained",side_effect=["failed",RuntimeError("live job")]),patch.object(execute,"run") as command:
            with self.assertRaisesRegex(RuntimeError,"live job"): execute.reset_drained_failures(["owned-a","owned-b"])
            command.assert_not_called()

    def test_setup_requires_reconciliation_before_input_replacement_or_unknown_dispatch(self):
        with patch.object(services,"verified"),patch.object(services,"stop",side_effect=AssertionError("replacement")):
            for state in ("prepared","starting","active","failed","drained"):
                with self.assertRaisesRegex(RuntimeError,"reconcile"):
                    services.update_owned({"setup":{"state":state}})
            with patch.object(services,"root_setup_withdrawn",return_value=False) as proof:
                with self.assertRaisesRegex(RuntimeError,"reconcile"):
                    services.update_owned({"setup":{"state":"failed","recovery_root_withdrawn":True}})
                proof.assert_called_once()
        with patch.object(services,"verified"),patch.object(services,"stop_unit",side_effect=AssertionError("unknown stop")):
            with self.assertRaisesRegex(RuntimeError,"unknown setup dispatch"):
                services.setup_reconcile({"setup":{"state":"starting"}})
        with patch.object(services,"save",side_effect=AssertionError("cleanup")):
            with self.assertRaisesRegex(RuntimeError,"holds its roots"):
                services.setup_cleanup_layout({"setup":{"launch":"a"*32,"state":"drained"}})

    def test_setup_selection_remains_short_lived_user_controller(self):
        for action in ("setup-prepare","setup-cancel","setup-busy"):
            record={"token":"00000000-0000-4000-8000-000000000001","owner":"00000000000040008000000000000002",
                    "pending":None,"units":[]}
            with patch.object(services,"save"),patch.object(services,"run",return_value="") as command, \
                 patch.object(services,"publish_projection_slot") as selected,patch.object(services,"publish_broker_slot") as broker:
                unit=services.start_unit(record,"user",probe="projection",launch_probe=action)
                selected.assert_called_once_with(record,unit); broker.assert_not_called()
                self.assertEqual(command.call_args.args[-1],"--"+action)
                self.assertIn("--property=CapabilityBoundingSet=",command.call_args.args)

    def test_launch_controller_cannot_become_broker_or_widen_action(self):
        record={"token":"00000000-0000-4000-8000-000000000001","owner":"00000000000040008000000000000002",
                "pending":None,"units":[]}
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            for role,probe,action,reservation in (("system","projection","prepare",False),("user",None,"prepare",False),
                    ("user","projection","arbitrary",False),("user","projection","cancel",True)):
                with self.assertRaises(ValueError): services.start_unit(record,role,probe=probe,launch_probe=action,reservation_probe=reservation)
        for action in ("prepare","cancel"):
            with patch.object(services,"save"),patch.object(services,"run",return_value="") as command, \
                 patch.object(services,"publish_projection_slot") as selected,patch.object(services,"publish_broker_slot") as broker:
                unit=services.start_unit(record,"user",probe="projection",launch_probe=action)
                selected.assert_called_once_with(record,unit); broker.assert_not_called()
                self.assertEqual(command.call_args.args[-1],"--launch-"+action)
                self.assertIn("--property=CapabilityBoundingSet=",command.call_args.args)
                self.assertFalse(any(value.startswith("--property=ReadWritePaths=") for value in command.call_args.args))

    def test_explicit_admission_is_normal_system_service_and_never_user_or_probe(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": []}
        with patch.object(services,"save"), patch.object(services,"run",return_value="") as command:
            unit=services.start_unit(record,"system",admit=True)
            self.assertIn("--service-type=notify",command.call_args.args)
            self.assertNotIn("--wait",command.call_args.args)
            self.assertEqual(command.call_args.args[-2:],("system","--admit-fixture"))
        record["active_units"]={"system":unit}
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))),record)
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError): services.start_unit(record,"user",admit=True)
            with self.assertRaises(ValueError): services.start_unit(record,"system",probe="projection",admit=True)

    def test_data_administration_never_selects_broker_or_empty_cleanup(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": []}
        for action in ("observe","enroll"):
            with patch.object(services,"save"), patch.object(services,"run",return_value="") as command, patch.object(services,"publish_broker_slot") as slot:
                unit=services.start_unit(record,"user",data=action)
                slot.assert_not_called()
                self.assertIn("--service-type=exec",command.call_args.args)
                self.assertEqual(command.call_args.args[-2:],("user","--data-"+action))
            self.assertEqual(services.validate_record(json.loads(json.dumps(record))),record)
            record["active_units"]={"user":unit}
            with self.assertRaisesRegex(RuntimeError,"cannot be an active authority"): services.validate_record(record)
            record.pop("active_units")
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError): services.start_unit(record,"system",data="enroll")
            with self.assertRaises(ValueError): services.start_unit(record,"user",data="enroll",probe="broker")
        with patch.object(services,"verified",side_effect=AssertionError("cleanup mutation")):
            with self.assertRaisesRegex(RuntimeError,"not an empty bootstrap"): services.cleanup({"app_data":{}})

    def test_projection_controller_is_separate_from_broker_and_cannot_widen_slot(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": [], "projection_unit": "-"}
        with patch.object(services,"save"), patch.object(services,"run",return_value="") as command, \
             patch.object(services,"publish_projection_slot") as slot, patch.object(services,"publish_broker_slot") as broker:
            unit=services.start_unit(record,"user",probe="projection")
            slot.assert_called_once_with(record,unit)
            broker.assert_not_called()
            self.assertIn("--property=SELinuxContext=system_u:system_r:aetheri_user_t:s0",command.call_args.args)
            self.assertIn("--property=CapabilityBoundingSet=",command.call_args.args)
            self.assertFalse(any(value.startswith("--property=ReadWritePaths=") for value in command.call_args.args))
            self.assertEqual(command.call_args.args[-1],str(services.ROOT/"probe-projection"))
        record["projection_unit"]=unit
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))),record)
        record["active_units"]={"user":unit}
        with self.assertRaisesRegex(RuntimeError,"cannot be an active authority"): services.validate_record(record)
        record.pop("active_units")
        with patch.object(services,"publish",side_effect=AssertionError("mutation")):
            for wrong in (unit.replace("-probe-projection-","-user-"),unit.replace("000000000001","000000000002"),"../unit"):
                with self.assertRaises(ValueError): services.publish_projection_slot(record,wrong)
        with patch.object(services,"publish"), patch.object(services,"flush_path"), patch.object(services.os,"replace"):
            services.publish_projection_slot(record,"-")
        self.assertEqual(record["projection_unit"],"-")
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError): services.start_unit(record,"system",probe="projection")

    def test_publication_uses_one_writable_mount_ancestor_and_administrative_identity(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": [], "fixture": {key: "00000000000040008000000000000002" for key in services.FIXTURE_IDS}}
        record["fixture"]["artifact"] = "ab" * 32
        for action in ("store-observe", "store-publish", "activate"):
            with patch.object(services, "save"), patch.object(services, "run", return_value="") as command:
                unit = services.start_unit(record, "system", fixture=action)
                arguments = command.call_args.args
                self.assertEqual([a for a in arguments if a.startswith("--property=ReadWritePaths=")], ["--property=ReadWritePaths=" + str(services.ROOT)])
                self.assertIn("--property=ReadOnlyPaths=" + str(services.fixture_paths(record)["source"]), arguments)
                self.assertIn("--property=CapabilityBoundingSet=", arguments)
                self.assertEqual(arguments[-2:], ("system", "--fixture-" + action))
            self.assertEqual(services.validate_record(json.loads(json.dumps(record))), record)
            record["active_units"] = {"system": unit}
            with self.assertRaisesRegex(RuntimeError, "cannot be an active authority"): services.validate_record(record)
            record.pop("active_units")
        with patch.object(services, "save", side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError): services.start_unit(record, "user", fixture="store-publish")

    def test_publication_payload_locations_must_be_unique(self):
        slot, target = self.root / "slot", self.root / "store/object"
        slot.mkdir()
        target.mkdir(parents=True)
        staging = slot / "staging"
        staging.mkdir()
        paths = dict(staging=staging, container=slot / "publication", target=target)
        self.assertEqual(services.fixture_payload({}, paths), staging)
        publication = {"fixture_publication": {}}
        staging.rename(target / "payload")
        self.assertEqual(services.fixture_payload(publication, paths), target / "payload")
        staging.mkdir()
        with self.assertRaisesRegex(RuntimeError, "ambiguous/missing"): services.fixture_payload(publication, paths)

    def test_storage_administration_cannot_become_a_live_broker(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": []}
        with patch.object(services, "save"), patch.object(services, "run", return_value="") as command, patch.object(services, "publish_broker_slot") as slot:
            unit = services.start_unit(record, "user", storage="publish")
            self.assertIn("--service-type=exec", command.call_args.args)
            self.assertEqual(command.call_args.args[-2:], ("user", "--storage-publish"))
            slot.assert_not_called()
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))), record)
        record["active_units"] = {"user": unit}
        with self.assertRaisesRegex(RuntimeError, "cannot be an active authority"):
            services.validate_record(record)
        with patch.object(services, "save", side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError):
                services.start_unit(record, "system", storage="publish")

    def test_allocated_private_data_is_preserved_by_empty_bootstrap_cleanup(self):
        with patch.object(services, "verified"), patch.object(services, "stop"), patch.object(services, "bus", side_effect=AssertionError("cleanup mutation")):
            with self.assertRaisesRegex(RuntimeError, "not an empty bootstrap"):
                services.cleanup({"pending": None, "storage": {}})

    def test_account_enrollment_receipt_roundtrip_and_role_boundary(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": []}
        with patch.object(services, "save"), patch.object(services, "run", return_value="") as command:
            unit = services.start_unit(record, "system", enroll=True)
            self.assertIn("--service-type=exec", command.call_args.args)
            self.assertEqual(command.call_args.args[-2:], ("system", "--enroll-account"))
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))), record)
        record["active_units"] = {"system": unit}
        with self.assertRaisesRegex(RuntimeError, "cannot be an active authority"):
            services.validate_record(record)
        record.pop("active_units")
        record["units"] = [unit.replace("-system-enroll-", "-user-enroll-")]
        with self.assertRaisesRegex(RuntimeError, "foreign unit"):
            services.validate_record(record)
        with patch.object(services, "save", side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError):
                services.start_unit(record, "user", enroll=True)

    def test_empty_bootstrap_cleanup_preserves_other_account_lifetimes(self):
        owner = "00000000000040008000000000000002"
        with sqlite3.connect(":memory:") as db:
            db.execute("CREATE TABLE scopes(scope_id BLOB,kind TEXT,os_uid INTEGER,state TEXT,revision INTEGER)")
            self.assertTrue(services.bootstrap_scopes(db, owner))
            db.execute("INSERT INTO scopes VALUES(?,'account',1000,'active',1)", (bytes.fromhex(owner),))
            self.assertTrue(services.bootstrap_scopes(db, owner))
            for column, value in (("scope_id", bytes(16)), ("kind", "machine"), ("os_uid", 1001), ("state", "retired"), ("revision", 2)):
                db.execute("SAVEPOINT changed_account")
                db.execute("UPDATE scopes SET " + column + "=?", (value,))
                self.assertFalse(services.bootstrap_scopes(db, owner))
                db.execute("ROLLBACK TO changed_account")
                db.execute("RELEASE changed_account")
            db.execute("INSERT INTO scopes VALUES(zeroblob(16),'account',1001,'active',1)")
            self.assertFalse(services.bootstrap_scopes(db, owner))

    def test_authority_pending_dispatch_blocks_start_stop_and_cleanup(self):
        record = {"pending": "unacknowledged.service"}
        with patch.object(services, "run", side_effect=AssertionError("supervisor mutation")), \
             patch.object(services, "verified", side_effect=AssertionError("queried installation")):
            for action in (services.start, services.stop, services.cleanup):
                with self.assertRaisesRegex(RuntimeError, "unacknowledged dispatch"):
                    action(record)

    def test_authority_dispatch_timeout_preserves_pending_identity(self):
        record = {"pending": None, "units": [], "token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002"}
        with patch.object(services, "save"), patch.object(services, "publish_broker_slot"), patch.object(services, "run", side_effect=subprocess.TimeoutExpired("systemd-run", 40)) as command:
            with self.assertRaises(subprocess.TimeoutExpired):
                services.start_unit(record, "user")
            self.assertEqual(record["pending"], record["units"][0])
            self.assertEqual(command.call_count, 1)

    def test_authority_stop_never_accepts_a_remaining_process(self):
        observations = ["ActiveState=active\nMainPID=123\nControlGroup=/owned\nLoadState=loaded", "", "ActiveState=failed\nMainPID=123\nControlGroup=/owned"]
        with patch.object(services, "run", side_effect=observations), patch.object(services, "save") as saved:
            with self.assertRaisesRegex(RuntimeError, "not drained"):
                services.stop({"pending": None, "units": ["owned.service"]})
            saved.assert_not_called()

    def test_authority_existing_name_with_wrong_pid_is_not_adopted(self):
        record = {"pending": None, "active_units": {"system": "owned.service"}}
        with patch.object(services, "verified"), patch.object(services, "bus", side_effect=["b true", "u 456"]), \
             patch.object(services, "run", return_value="123"), patch.object(services, "start_unit") as dispatch:
            with self.assertRaisesRegex(RuntimeError, "identity is unresolved"):
                services.start(record)
            dispatch.assert_not_called()

    def test_authority_stop_preserves_a_pending_supervisor_job(self):
        observations = ["ActiveState=inactive\nMainPID=0\nControlGroup=\nLoadState=loaded", "", "ActiveState=inactive\nMainPID=0\nControlGroup="]
        jobs = {"type": "a(usssoo)", "data": [[[1, "owned.service", "start", "waiting", "/job/1", "/unit/1"]]]}
        with patch.object(services, "run", side_effect=observations), patch.object(services, "bus", return_value=json.dumps(jobs)), patch.object(services, "save") as saved:
            with self.assertRaisesRegex(RuntimeError, "supervisor job"):
                services.stop({"pending": None, "units": ["owned.service"]})
            saved.assert_not_called()

    def test_authority_failed_dispatch_with_a_job_stays_pending(self):
        record = {"pending": None, "units": [], "token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002"}
        failed = subprocess.CalledProcessError(1, "systemd-run", stderr="failed")
        with patch.object(services, "save"), patch.object(services, "run", side_effect=[failed, "ActiveState=failed\nMainPID=0\nControlGroup="]), \
             patch.object(services, "unit_has_job", return_value=True), patch.object(services, "publish_broker_slot"):
            with self.assertRaises(subprocess.CalledProcessError):
                services.start_unit(record, "user")
            self.assertEqual(record["pending"], record["units"][0])

    def test_authority_stopped_unit_with_pending_job_is_not_replaced(self):
        record = {"pending": None, "active_units": {"system": "owned.service"}}
        with patch.object(services, "verified"), patch.object(services, "bus", return_value="b false"), \
             patch.object(services, "run", return_value="ActiveState=inactive\nMainPID=0\nControlGroup="), \
             patch.object(services, "unit_has_job", return_value=True), patch.object(services, "start_unit") as dispatched:
            with self.assertRaisesRegex(RuntimeError, "still live/unknown"):
                services.start(record)
            dispatched.assert_not_called()

    def test_policy_cleanup_never_infers_exit_from_unacknowledged_dispatch(self):
        with patch.object(policy, "run", side_effect=AssertionError("queried or mutated supervisor")) as invoked:
            with self.assertRaisesRegex(RuntimeError, "unacknowledged dispatch"):
                policy.cleanup_owned({"dispatch_in_progress": "uncertain.service"})
            invoked.assert_not_called()

    def setUp(self):
        output = vm.ROOT / "out"
        output.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="vm-tools-", dir=output)
        self.root = Path(self.temp.name)
        if self.root.resolve().parent != output.resolve():
            raise ValueError("test cleanup outside workspace")
        self.addCleanup(self.temp.cleanup)
        self.guest = self.root / "vm/p0"
        self.guest.mkdir(parents=True)
        for name, value in (("ROOT", self.root), ("VM", self.guest), ("TOOLS", self.root / "tools")):
            guard = patch.object(vm, name, value)
            guard.start()
            self.addCleanup(guard.stop)
        self.registration = {"uuid": "00000000-0000-4000-8000-000000000001",
                             "disk_bytes": 48 * 1024**3, "disk_serial": "AETHERI_P0_ONLY"}
        (self.guest / "config.json").write_text(json.dumps(self.registration), encoding="utf-8")

    def test_wrong_qmp_vm_is_not_accepted(self):
        with patch.object(vm, "qmp", return_value={"UUID": "ffffffff-ffff-4fff-8fff-ffffffffffff"}):
            with self.assertRaises(ValueError):
                vm.running()

    def test_correct_qmp_identity_and_observation_timeout(self):
        with patch.object(vm, "qmp", return_value={"UUID": self.registration["uuid"]}):
            self.assertTrue(vm.running())
        with patch.object(vm, "qmp", side_effect=TimeoutError):
            self.assertFalse(vm.running())  # Does not prove process exit; start checks PID too.

    def test_live_or_unresolved_start_never_dispatches_again(self):
        for record in ({"pid": 12345}, {"state": "starting"}):
            (self.guest / "process.json").write_text(json.dumps(record), encoding="utf-8")
            with patch.object(vm, "running", return_value=False), \
                 patch.object(vm, "pid_alive", return_value=True), \
                 patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
                with self.assertRaises(ValueError):
                    vm.start(False)

    def test_running_guest_is_not_reinstalled(self):
        with patch.object(vm, "running", return_value=True), \
             patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
            with self.assertRaises(ValueError):
                vm.start(True, retry=True)

    def test_repeat_install_needs_explicit_flag_even_after_process_exit(self):
        (self.guest / "process.json").write_text('{"pid":12345}', encoding="utf-8")
        with patch.object(vm, "running", return_value=False), \
             patch.object(vm, "pid_alive", return_value=False), \
             patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
            with self.assertRaises(ValueError):
                vm.start(True)

    def test_unregistered_disk_is_not_adopted(self):
        (self.guest / "config.json").unlink()
        disk = self.guest / "system.qcow2"
        disk.write_bytes(b"existing data")
        with self.assertRaises(ValueError):
            vm.prepare()
        self.assertEqual(disk.read_bytes(), b"existing data")

    def test_firmware_and_disk_are_local_and_network_can_be_restricted(self):
        command = vm.command(False, offline=True)
        self.assertIn("host,vmx=off,svm=off", command)
        self.assertIn("8192M", command)
        net = command[command.index("-netdev") + 1]
        self.assertIn("restrict=on", net)
        self.assertIn("127.0.0.1", net)
        self.assertNotIn("-fsdev", command)
        self.assertNotIn("-virtfs", command)
        drives = [command[i + 1] for i, item in enumerate(command) if item == "-drive"]
        self.assertEqual(len(drives), 3)
        self.assertTrue(all(str(self.root) in drive for drive in drives))
        self.assertTrue(any("readonly=on" in drive and "pflash" in drive for drive in drives))
        self.assertTrue(any("format=qcow2" in drive for drive in drives))
        monitor = command[command.index("-qmp") + 1]
        self.assertTrue(monitor.startswith("unix:"))
        self.assertNotIn("tcp:", monitor)

    def test_offline_install_is_rejected_before_mutation(self):
        with patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
            with self.assertRaises(ValueError):
                vm.start(True, offline=True)

    def test_atomic_state_preserves_previous_file_when_temp_already_exists(self):
        path = self.guest / "process.json"
        path.write_text("previous", encoding="utf-8")
        path.with_suffix(".json.tmp").write_text("interrupted", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            vm.write_state(path, {"pid": 1})
        self.assertEqual(path.read_text(), "previous")

    def test_provision_reuses_equal_bytes_and_rejects_changed_existing_tool(self):
        source, target = self.root / "source", self.root / "target"
        source.mkdir()
        target.mkdir()
        (source / "tool").write_bytes(b"fixture")
        with patch.object(provision, "ROOT", self.root):
            receipt = provision.publish_files(source, target)
            self.assertEqual(provision.publish_files(source, target), receipt)
            (target / "tool").write_bytes(b"changed")
            with self.assertRaises(ValueError):
                provision.publish_files(source, target)
            self.assertEqual((target / "tool").read_bytes(), b"changed")


if __name__ == "__main__":
    unittest.main(verbosity=2)
