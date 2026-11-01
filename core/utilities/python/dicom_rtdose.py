import pydicom
import numpy as np
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.uid import UID, ExplicitVRLittleEndian, generate_uid
from pydicom.sequence import Sequence

def write_rtdose(dicom_metadata: Dataset, grid_meta: dict, dose_grid, output_file): #new
    """
    Create RTdose file.
    """
    dose_arr = np.array(dose_grid, copy=True)

    x_unique = grid_meta['x_unique']
    y_unique = grid_meta['y_unique']
    z_unique = grid_meta['z_unique']

    number_of_frames, rows, columns = dose_arr.shape
    
    ds = dicom_metadata.copy()

    # --- File Meta Information ---
    file_meta = FileMetaDataset()
    file_meta.MediaStorageSOPClassUID = UID("1.2.840.10008.5.1.4.1.1.481.2")  # RT Dose Storage
    file_meta.MediaStorageSOPInstanceUID = generate_uid()
    file_meta.TransferSyntaxUID = ExplicitVRLittleEndian

    # Przepisanie ImplementationClassUID z bazy lub pydicom domyślny
    if hasattr(dicom_metadata, "file_meta") and hasattr(dicom_metadata.file_meta, "ImplementationClassUID"):
        file_meta.ImplementationClassUID = dicom_metadata.file_meta.ImplementationClassUID
    else:
        file_meta.ImplementationClassUID = pydicom.uid.PYDICOM_IMPLEMENTATION_UID

    ds.file_meta = file_meta
    ds.is_little_endian = True
    ds.is_implicit_VR = False

    # --- SOP Instance / Class ---
    ds.SOPClassUID = file_meta.MediaStorageSOPClassUID
    ds.SOPInstanceUID = file_meta.MediaStorageSOPInstanceUID

    # --- RTDose Specific Attributes ---
    ds.Modality = 'RTDOSE'
    ds.DoseUnits = 'GY'
    ds.Rows = rows
    ds.Columns = columns
    ds.NumberOfFrames = number_of_frames

    max_dose = max(dose_arr.max(), 0)
    ds.DoseGridScaling = f"{max_dose / 65535:.8e}"

    ds.BitsAllocated = 16
    ds.BitsStored = 16
    ds.HighBit = 15
    ds.PixelRepresentation = 0

    z_start = float(z_unique[0])
    ds.GridFrameOffsetVector = [float(z) - z_start for z in z_unique]
    ds.ImagePositionPatient = [float(x_unique[0]), float(y_unique[0]), z_start]
    ds.PixelSpacing = [float(x_unique[1] - x_unique[0]), float(y_unique[1] - y_unique[0])]
    ds.SliceThickness = abs(float(z_unique[1] - z_unique[0])) if len(z_unique) > 1 else 1.0
    ds.ImageOrientationPatient = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0]

    normalized_dose = dose_arr / max_dose
    scaled_dose = (normalized_dose * 65535).clip(0, 65535)
    ds.PixelData = scaled_dose.astype(np.uint16).tobytes()

    # -- Series Information --
    study_prefix = getattr(ds, "StudyInstanceUID", None)
    ds.SeriesInstanceUID = generate_uid()
    ds.SeriesNumber = '2'
    ds.SamplesPerPixel = 1
    ds.PhotometricInterpretation = "MONOCHROME2"
    ds.FrameIncrementPointer = (0x3004, 0x000C)

    # Reference to RT_Plan (TODO: connect to correct one)
    referenced_rt_plan = Dataset()
    referenced_rt_plan.ReferencedSOPClassUID = "1.2.840.10008.5.1.4.1.1.481.5"
    referenced_rt_plan.ReferencedSOPInstanceUID = "1.2.3.4.5.6.7.8.9.0"
    ds.ReferencedRTPlanSequence = Sequence([referenced_rt_plan])

    ds.DoseSummationType = 'PLAN'
    ds.DoseType = 'PHYSICAL'

    print("===== Saved to file =====")
    ds.save_as(output_file, enforce_file_format=True)
