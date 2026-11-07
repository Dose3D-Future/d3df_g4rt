import json
import datetime
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.uid import generate_uid
from pydicom.datadict import keyword_dict, dictionary_VR

MAX_VR_LEN = {
    "SH": 16,    # Short String
    "LO": 64,    # Long String
    "ST": 1024,  # Short Text
    "LT": 10240, # Long Text
    "PN": 64,    # Person Name
    "UI": 64,    # Unique Identifier
    "CS": 16,    # Code String
    "AE": 16,    # Application Entity
    "DS": 16,    # Decimal String
    "IS": 12,    # Integer String
    "DA": 8,     # Date
    "TM": 16,    # Time
    "DT": 26     # Date Time
}

def generate_base_metadata(patient_metadata_path: str, 
                           patient_name: str,
                           patient_id_hash: str,
                           app_version: str,
                           job_name_hash: str,
                           patient_description: str = ""
                           ) -> Dataset:
    ds = Dataset()
    ds.file_meta = FileMetaDataset()
    
    with open(patient_metadata_path, 'r', encoding='utf-8') as f:
        template = json.load(f)
        
    now = datetime.datetime.now()
    dicom_uid_root = template.get("DicomUIDRoot", "1.2.840.10008")
    org_uid_root = template.get("OrganizationUID", "")

    replacements = {
        "_PATIENT_GEOMETRY_TYPE_": patient_name,
        "_STUDY_DESCRIPTION_": patient_description,
        "_PATIENT_ID_HASH_": patient_id_hash,
        "_STUDY_DATE_": now.strftime("%Y%m%d"),
        "_STUDY_TIME_": now.strftime("%H%M%S"),
        "_G4RT_VERSION_": app_version,
        "_JOB_NAME_HASH_": job_name_hash,
        "_DICOM_UID_ROOT_": dicom_uid_root,
        "_ORGANIZATION_UID_": org_uid_root
    }

    for block_name, block_data in template.items():
        if block_name in ["DicomUIDRoot", "OrganizationUID"]:
            continue

        for key, val in block_data.items():
            final_val = val
            if isinstance(final_val, str):
                for old_val, new_val in replacements.items():
                    final_val = final_val.replace(old_val, str(new_val))
                for uid_suffix in ["_SERIES_SUFFIX_", "_SUFFIX_"]:
                    if uid_suffix in final_val:
                        prefix = final_val.replace(uid_suffix, "")
                        final_val = generate_uid(prefix=prefix)
                        break
            # lenght validation
            tag_hex = keyword_dict.get(key)
            if tag_hex is not None:
                vr = dictionary_VR(tag_hex)
                if vr in MAX_VR_LEN:
                    max_len = MAX_VR_LEN[vr]
                    if len(final_val) > max_len:
                        raise ValueError(f"Error: You inserted a string that is too long into {key} (type {vr}). ")
            if block_name == "FileMetaInformation":
                setattr(ds.file_meta, key, final_val)
            else:
                setattr(ds, key, final_val)
            
    return ds