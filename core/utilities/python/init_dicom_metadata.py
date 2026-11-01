import json
import datetime
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.uid import generate_uid

def generate_base_metadata(patient_metadata_path: str, 
                           patient_name: str, 
                           patient_id: str, 
                           study_id: str, 
                           app_version: str
                           ) -> Dataset:
   ds = Dataset()
   ds.file_meta = FileMetaDataset()
   
   with open(patient_metadata_path, 'r', encoding='utf-8') as f:
      template = json.load(f)
      
   now = datetime.datetime.now()

   for key, val in template.items():
      if key.startswith("_COMMENT"):
         continue

      if key == "FileMetaInformation" and isinstance(val, dict):
         for meta_key, meta_val in val.items():
            if meta_key.startswith("_COMMENT"):
               continue
            setattr(ds.file_meta, meta_key, meta_val)
         continue
      if key == "StudyInstanceUID":
         extracted_prefix = val.replace("_SUFFIX_", "") 
         final_val = generate_uid(prefix=extracted_prefix)
      elif val == "_PATIENT_NAME_":
         final_val = patient_name
      elif val == "_STUDY_DESCRIPTION_":
         final_val = patient_name  
      elif val == "_PATIENT_ID_HASH_":
         final_val = patient_id
      elif val == "_STUDY_DATE_":
         final_val = now.strftime("%Y%m%d")
      elif val == "_STUDY_TIME_":
         final_val = now.strftime("%H%M%S")
      elif val == "_D3D_STUDY_":
         final_val = study_id
      elif val == "_G4RT_VERSION_":
         final_val = app_version
      else:
         final_val = val
         
      setattr(ds, key, final_val)
      
   return ds