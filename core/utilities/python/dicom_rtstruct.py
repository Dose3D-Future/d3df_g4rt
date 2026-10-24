import datetime
import glob
import os
from pathlib import Path
import pydicom
import pydicom.uid
from pydicom.dataset import Dataset, FileDataset, FileMetaDataset
from pydicom.uid import ImplicitVRLittleEndian


class RtStructSvc:
    __output_path = None
    __project_path = None

    def __init__(self, label="RTSTRUCT_"):
        print("Initializing RT-Struct generator.")
        self.__label = label

    @classmethod
    def set_output_path(cls, output_path):
        cls.__output_path = output_path
        if cls.__output_path[-1] != "/":
            cls.__output_path = cls.__output_path + "/"
        os.makedirs(cls.__output_path, exist_ok=True)
        print(f"RT-Struct Output was set to: {output_path}")

    @classmethod
    def set_project_path(cls, project_path):
        cls.__project_path = project_path
        if cls.__project_path[-1] != "/":
            cls.__project_path = cls.__project_path + "/"
        os.makedirs(cls.__project_path, exist_ok=True)
        print(f"RT-Struct Project root was set to: {project_path}")

    def create_rtstruct_from_cells(self, ct_series_path: str, cells_list: list):
        print("Starting RT-Struct creation...")

        ct_files = sorted(glob.glob(os.path.join(ct_series_path, "*.dcm")))
        if not ct_files:
            raise RuntimeError(f"No CT DICOM files found in path: {ct_series_path}")

        ct_datasets = [pydicom.dcmread(f) for f in ct_files]

        ref_ct = ct_datasets[0]

        # file Metadata
        out_full_path = os.path.join(self.__output_path, "rtstruct.dcm")
        file_meta = FileMetaDataset()
        file_meta.MediaStorageSOPClassUID = "1.2.840.10008.5.1.4.1.1.481.3"
        file_meta.MediaStorageSOPInstanceUID = pydicom.uid.generate_uid(prefix="1.2.826.0.1.3680043.8.498.997.")
        file_meta.ImplementationClassUID = "1.2.826.0.1.3680043.8.498.997"
        file_meta.ImplementationVersionName = "pydicom 3.0.1"
        file_meta.TransferSyntaxUID = ImplicitVRLittleEndian
        ds = FileDataset(out_full_path, {}, file_meta=file_meta, preamble=b"\0" * 128)

        ds.PatientName = getattr(ref_ct, "PatientName", "Dose3D_Phantom")
        ds.PatientID = getattr(ref_ct, "PatientID", "0123456789")
        ds.PatientBirthDate = getattr(ref_ct, "PatientBirthDate", "20000101")
        ds.StudyInstanceUID = ref_ct.StudyInstanceUID
        ds.StudyID = getattr(ref_ct, "StudyID", "1")
        ds.SeriesInstanceUID = pydicom.uid.generate_uid(prefix="1.2.826.0.1.3680043.8.498.997.")
        ds.SOPClassUID = file_meta.MediaStorageSOPClassUID
        ds.SOPInstanceUID = file_meta.MediaStorageSOPInstanceUID
        ds.Modality = "RTSTRUCT"
        ds.StructureSetLabel = "D3DF_CELLS"
        ds.StructureSetName = "SensitiveCells"

        now = datetime.datetime.now()
        ds.StructureSetDate = now.strftime("%Y%m%d")
        ds.StructureSetTime = now.strftime("%H%M%S")

        # Reference to CT
        ref_fo_r = Dataset()
        ref_fo_r.FrameOfReferenceUID = getattr(ref_ct, "FrameOfReferenceUID", "1.2.840.10008.15.1.1")
        rt_ref_study = Dataset()
        rt_ref_study.ReferencedSOPClassUID = "1.2.840.10008.3.1.2.3.1"
        rt_ref_study.ReferencedSOPInstanceUID = ref_ct.StudyInstanceUID
        rt_ref_series = Dataset()
        rt_ref_series.SeriesInstanceUID = ref_ct.SeriesInstanceUID
        contour_image_seq = []
        for ct in ct_datasets:
            img_item = Dataset()
            img_item.ReferencedSOPClassUID = ct.SOPClassUID
            img_item.ReferencedSOPInstanceUID = ct.SOPInstanceUID
            contour_image_seq.append(img_item)
        rt_ref_series.ContourImageSequence = contour_image_seq
        rt_ref_study.RTReferencedSeriesSequence = [rt_ref_series]

        ref_fo_r.RTReferencedStudySequence = [rt_ref_study]
        ds.ReferencedFrameOfReferenceSequence = [ref_fo_r]

        # Cells
        ds.StructureSetROISequence = []
        ds.ROIContourSequence = []
        ds.RTROIObservationsSequence = []
        colors = [
            [255, 0, 0], [0, 255, 0], [0, 0, 255],
            [255, 255, 0], [255, 0, 255], [0, 255, 255],
            [255, 128, 0], [128, 0, 255]
        ]

        print(f"Processing {len(cells_list)} cells into RT-Struct...")
        for idx, cell in enumerate(cells_list, start=1):
            label = cell["label"]
            dx, dy, dz = cell["halflength"]
            voxel_centers = cell["contourPoints"]

            ss_roi = Dataset()
            ss_roi.ROINumber = idx
            ss_roi.ReferencedFrameOfReferenceUID = ref_fo_r.FrameOfReferenceUID
            ss_roi.ROIName = label
            ss_roi.ROIGenerationAlgorithm = "AUTOMATIC"
            ds.StructureSetROISequence.append(ss_roi)

            roi_contour = Dataset()
            roi_contour.ReferencedROINumber = idx
            roi_contour.ROIDisplayColor = colors[(idx - 1) % len(colors)]
            roi_contour.ContourSequence = []

            for ct in ct_datasets:
                pos = [float(x) for x in ct.ImagePositionPatient]
                slice_z = pos[2]
                intersecting_voxels = [
                    pt for pt in voxel_centers 
                    if (pt[2] - dz) <= slice_z <= (pt[2] + dz)
                ]
                if not intersecting_voxels:
                    continue
                min_cx = min(pt[0] for pt in intersecting_voxels)
                max_cx = max(pt[0] for pt in intersecting_voxels)
                min_cy = min(pt[1] for pt in intersecting_voxels)
                max_cy = max(pt[1] for pt in intersecting_voxels)

                x_min = min_cx - dx
                x_max = max_cx + dx
                y_min = min_cy - dy
                y_max = max_cy + dy

                c_item = Dataset()
                c_item.ContourGeometricType = "CLOSED_PLANAR"
                c_item.ContourData = [
                    x_min, y_min, slice_z,
                    x_max, y_min, slice_z,
                    x_max, y_max, slice_z,
                    x_min, y_max, slice_z
                ]
                c_item.NumberOfContourPoints = 4

                ref_slice = Dataset()
                ref_slice.ReferencedSOPClassUID = ct.SOPClassUID
                ref_slice.ReferencedSOPInstanceUID = ct.SOPInstanceUID
                c_item.ContourImageSequence = [ref_slice]

                roi_contour.ContourSequence.append(c_item)

            ds.ROIContourSequence.append(roi_contour)

            # Observation of ROI
            obs = Dataset()
            obs.ObservationNumber = idx
            obs.ReferencedROINumber = idx
            obs.ROIInterpretedType = "ORGAN"
            obs.ROIInterpreter = ""
            ds.RTROIObservationsSequence.append(obs)

        ds.is_little_endian = True
        ds.is_implicit_VR = True
        ds.save_as(out_full_path, write_like_original=False)
        print(f"Successfully saved RT-Struct to: {out_full_path}")