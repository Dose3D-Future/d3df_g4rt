import os
import json
import datetime
import pydicom
from pydicom.uid import PYDICOM_IMPLEMENTATION_UID

def get_dicom_metadata(base_json_path: str, run_json_path: str) -> dict:
    """
    Loads the base JSON file and calculates dynamic metadata (dates, UIDs).
    
    Args:
        base_json_path: Path to the generic_patient.json.
        run_json_path: Path to the temporary file for the current run.
    """
    if os.path.exists(run_json_path):
        print(f"-> Loading existing run metadata from: {run_json_path}")
        with open(run_json_path, 'r', encoding='utf-8') as f:
            return json.load(f)

    print(f"-> Generating new run metadata from: {base_json_path}")
    with open(base_json_path, 'r', encoding='utf-8') as f:
        base_meta = json.load(f)

    now = datetime.datetime.now()
    study_date = now.strftime("%Y%m%d")
    study_time = now.strftime("%H%M%S")
    
    uid_prefix = "1.2.826.0.1.3680043.8.498.997."

    metadata = {
        # --- PATIENT DATA ---
        "PatientName": base_meta.get("PatientName", "Dose3D_Phantom"),
        "PatientID": base_meta.get("PatientID", "0123456789"),
        "PatientBirthDate": base_meta.get("PatientBirthDate", "20000101"),
        "PatientSex": base_meta.get("PatientSex", "O"),
        
        # --- INSTITUTION / EQUIPMENT DATA ---
        "Manufacturer": base_meta.get("Manufacturer", "WFiIS_AGH"),
        "ManufacturerModelName": base_meta.get("ManufacturerModelName", "DICOMaker"),
        "SoftwareVersions": base_meta.get("SoftwareVersions", "v1.0"),
        "InstitutionName": base_meta.get("InstitutionName", "AGH WFiIS"),
        "InstitutionAddress": base_meta.get("InstitutionAddress", "Krakow"),
        "ReferringPhysicianName": base_meta.get("ReferringPhysicianName", ""),
        "OperatorsName": base_meta.get("OperatorsName", "Dose3D_Operator"),
        
        # --- FILE META DATA ---
        "ImplementationClassUID": base_meta.get("ImplementationClassUID", PYDICOM_IMPLEMENTATION_UID),
        "ImplementationVersionName": base_meta.get("ImplementationVersionName", "pydicom"),
        "SourceApplicationEntityTitle": base_meta.get("SourceApplicationEntityTitle", "Dose-3D"),

        # --- STUDY DATA (SHARED ACROSS ALL FILES) ---
        "StudyID": base_meta.get("StudyID", "1"),
        "StudyDescription": base_meta.get("StudyDescription", ""),
        "AccessionNumber": base_meta.get("AccessionNumber", ""),
        "StudyDate": study_date,
        "StudyTime": study_time,
        
        # --- CRITICAL: Generated ONCE and saved for the entire run ---
        "StudyInstanceUID": pydicom.uid.generate_uid(prefix=uid_prefix),
        
        # --- GEOMETRY AND POSITIONING (SHARED) ---
        "FrameOfReferenceUID": base_meta.get("FrameOfReferenceUID", "1.2.840.10008.15.1.1"),
        "PositionReferenceIndicator": base_meta.get("PositionReferenceIndicator", "XZ"),
        "PatientPosition": base_meta.get("PatientPosition", "HFS"),

        # --- DATES / TIMES FOR SPECIFIC SERIES ---
        "SeriesDate": study_date,
        "SeriesTime": study_time,
        "ContentDate": study_date,

        # --- UNIQUE UIDS FOR SERIES (Each modality needs a separate series) ---
        "SeriesInstanceUID_CT": pydicom.uid.generate_uid(prefix=uid_prefix),
        "SeriesInstanceUID_RTDose": pydicom.uid.generate_uid(prefix=uid_prefix),
        "SeriesInstanceUID_RTStruct": pydicom.uid.generate_uid(prefix=uid_prefix),
        
        # --- SERIES DESCRIPTIONS ---
        "SeriesDescription_CT": base_meta.get("SeriesDescription_CT", "CT Series"),
    }
    run_dir = os.path.dirname(os.path.abspath(run_json_path))
    if run_dir:
        os.makedirs(run_dir, exist_ok=True)
        
    with open(run_json_path, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, indent=4)
        print(f"-> Run metadata successfully saved to: {run_json_path}")
        
    return metadata