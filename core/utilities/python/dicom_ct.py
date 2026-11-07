import datetime
# from loguru import logger
import os
import time
from sys import prefix
import time
import numpy as np
import pandas as pd
import pydicom
import pydicom.uid
from pydicom.dataset import Dataset, FileDataset
from pydicom.uid import ExplicitVRLittleEndian
import json
from pathlib import Path

# def CtSvc():
#     logger.info("Initializing CT scaner.")

# def create_ct_series():
#     logger.info("Initializing CT series.")

# def set_output_path():
#     logger.info("Setting path for CT series.")


class CtSvc():
    __output_path = None
    __project_path = None

# -------------------------------- Init -----------------------------------------
    def __init__(self, label="CT_"):
        print("Initializing CT scaner.")
        print()
        self.__label = label # Did I Need that?
        self.__generate_CT = True # Did I Need that?
        self.__output_array = np.zeros((1, 1, 1))
        self.__data_path = ""
        self.__plain_data = np.zeros(shape = (1*1*1, 3, ))
        self.__data_array  = pd.DataFrame(self.__plain_data,columns=['x','y','z'])
        self.__x_min = 0
        self.__x_max = 0
        self.__y_min = 0
        self.__y_max = 0
        self.__z_min = 0
        self.__z_max = 0
        self.__pixel_in_x = 0
        self.__pixel_in_y = 0
        self.__pixel_in_z = 0
        self.__step_x = 0
        self.__step_y = 0
        self.__step_z = 0
        self.__SSD = 0

# -------------------------------- User Interface Methods -----------------------

    @classmethod
    def set_output_path(cls, output_path):

        cls.__output_path = output_path
        if cls.__output_path[-1] != "/":
            cls.__output_path=cls.__output_path+"/"
        try:
            os.makedirs(cls.__output_path)
        except FileExistsError:
            # directory already exists
            pass
        print(f"Output was set to: {output_path}")

    @classmethod
    def set_project_path(cls, project_path):
        # Not as set_hounsfield_dictiobnary cause also it sett path to template CT.dcm
        cls.__project_path = project_path
        if cls.__project_path[-1] != "/":
            cls.__project_path=cls.__project_path+"/"
        try:
            os.makedirs(cls.__project_path)
        except FileExistsError:
            # directory already exists
            pass
        print(f"Project root was set to: {project_path}")

    def  __set_hounsfield_dictiobnary(self):
        dictionary = (f"{self.__project_path}config/hounsfield_scale_120keV.json")
        with open(dictionary) as jsn_file:
            self.__hounsfield_units_dictionary, self.__image_type_dictionary = json.load(jsn_file)
# ------------------------------- Priv Class Methods ----------------------------


    def __set_metadata_from_dict(self, cfg):
        self.__x_min = round(float(cfg["initX"]), 4)
        self.__y_min = round(float(cfg["initY"]), 4)
        self.__z_min = round(float(cfg["initZ"]), 4)
        self.__pixel_in_x = round(float(cfg["xRes"]), 4)
        self.__pixel_in_y = round(float(cfg["yRes"]), 4)
        self.__pixel_in_z = round(float(cfg["zRes"]), 4)
        self.__step_x = round(float(cfg["sizeX"]), 4)
        self.__step_y = round(float(cfg["sizeY"]), 4)
        self.__step_z = round(float(cfg["sizeZ"]), 4)
        self.__x_max = round(self.__x_min + self.__step_x * self.__pixel_in_x, 4)
        self.__y_max = round(self.__y_min + self.__step_y * self.__pixel_in_y, 4)
        self.__z_max = round(self.__z_min + self.__step_z * self.__pixel_in_z, 4)
        self.__SSD = round(float(cfg.get("SSD", 1000.0)), 4)

    def __start_Dicom_series(self):
        self.__set_hounsfield_dictiobnary()
        name = f"{self.__project_path}config/ct_slice_template.dcm"
        ds = pydicom.dcmread(name)

        # --------------- overwrite metadata ----------------
        ds.file_meta.MediaStorageSOPInstanceUID = pydicom.uid.generate_uid(prefix="1.2.826.0.1.3680043.8.498.997.")
        ds.file_meta.ImplementationClassUID = "1.2.826.0.1.3680043.8.498.997"
        ds.file_meta.ImplementationVersionName = "pydicom 3.0.1"
        ds.file_meta.SourceApplicationEntityTitle = "Dose-3D"

        # -------------- overwrite flesh ------------------
        ds.SOPClassUID = ds.file_meta.MediaStorageSOPClassUID
        ds.SOPInstanceUID = ds.file_meta.MediaStorageSOPInstanceUID
        ds.StudyTime = "123030"
        ds.SeriesTime = "123100"
        ds.StudyDate = datetime.datetime.now().strftime('%Y%m%d')
        # ds.StudyDate = datetime.datetime.now().strftime('%Y%m%d')
        ds.SeriesDate = datetime.datetime.now().strftime('%Y%m%d')
        # ds.SeriesDate = datetime.datetime.now().strftime('%Y%m%d')
        ds.ContentDate = datetime.datetime.now().strftime('%Y%m%d')
        # ds.ContentDate = datetime.datetime.now().strftime('%Y%m%d')
        ds.Manufacturer = "Dose3D"
        ds.InstitutionName = "AGH WFiIS"
        ds.InstitutionAddress = "Kraków"
        ds.SeriesDescription = "Test slice of CT"
        ds.ManufacturerModelName = "DICOMaker v. alpha v0.19.33"
        ds.PatientName = self.__label
        ds.StudyDescription = "Describe me!"
        ds.PatientID = "0123456789"
        ds.PatientBirthDate = '20000101'
        ds.SoftwareVersions = "DICOMaker v. alpha v0.19.33"
        # Should it be set at same distance?
        ds.DistanceSourceToDetector = self.__SSD
        ds.DistanceSourceToPatient = self.__SSD
        # ds.TableHeight = "To remove"
        # ds.RotationDirection = "To remove"
        # ds.ExposureTime = "0"
        # ds.XRayTubeCurrent = "0"
        # ds.Exposure = "0"
        # ds.FilterType = "0"
        # ds.GeneratorPower = "0"
        ds.StudyInstanceUID = pydicom.uid.generate_uid(prefix="1.2.826.0.1.3680043.8.498.997.")
        ds.SeriesInstanceUID  = pydicom.uid.generate_uid(prefix="1.2.826.0.1.3680043.8.498.997.")
        ds.StudyID = "1"
        ds.SeriesNumber = "1"
        ds.FrameOfReferenceUID = "1.2.840.10008.15.1.1"
        ds.PositionReferenceIndicator = "XZ"
        ds.ImageOrientationPatient = r"1\0\0\0\1\0"
        ds.PatientPosition = "HFS"
        ds.PixelRepresentation = 1
        ds.WindowCenter = r"125.0"
        ds.WindowWidth = r"600.0"
        ds.RescaleIntercept = r"0.0"
        return ds

    def __write_Dicom_ct_slice(self, rawdata, sliceNumber):
        image2d = rawdata.astype(np.uint16)
        ds = self.series
        ds.file_meta.MediaStorageSOPInstanceUID = pydicom.uid.generate_uid(prefix="1.2.826.0.1.3680043.8.498.997.1997.")
        ds.SOPInstanceUID = ds.file_meta.MediaStorageSOPInstanceUID
        ds.Rows = image2d.shape[0]
        ds.Columns = image2d.shape[1]

        ds.SliceThickness = f"{self.__step_y}"

        ds.PixelSpacing = f"{self.__step_x}\{self.__step_z}"


        ds.ImagePositionPatient = f"{self.__x_min}\{self.__z_min}\{round(self.__y_min+sliceNumber*self.__step_y,4)}"


        ds.SliceLocation = f"{round(self.__y_min+sliceNumber*self.__step_y,4)}"

        ds.InstanceCreationTime = datetime.datetime.now().strftime("%H%M%S.%f")[:-3]
        ds.ContentTime = datetime.datetime.now().strftime("%H%M%S")
        ds.InstanceNumber = sliceNumber
        ds.PixelData = image2d.tobytes()
        pydicom.dataset.validate_file_meta(ds.file_meta, enforce_standard=True)
        ds.save_as(self.__output_path+self.__label+f"{sliceNumber}"+r".dcm")
        print(f"I just saved {self.__label}{sliceNumber}.dcm")

    def __write_whole_Dicom_ct(self, voxel_materials):
        self.series = self.__start_Dicom_series()
        self.instance_UID = self.series.SOPInstanceUID
        
        print("Start iteration over images in memory")
        
        y_res = int(self.__pixel_in_y)
        x_res = int(self.__pixel_in_x)
        z_res = int(self.__pixel_in_z)
        slice_size = x_res * z_res
        
        print(f"Generating {y_res} CT slices...")
        
        for y in range(y_res):
            start_idx = y * slice_size
            end_idx = start_idx + slice_size
            slice_materials = voxel_materials[start_idx:end_idx]
            
            image_data = (
                pd.Series(slice_materials)
                .map(self.__hounsfield_units_dictionary)
                .values
                .reshape(
                    z_res,
                    x_res,
                    order="F"
                )
            )
            
            iterator = y + 1
            self.__write_Dicom_ct_slice(image_data, iterator)
# -------------------------------- Public Methods --------------------------------
    def create_ct_series(self, cfg, voxel_materials):
        self.__set_metadata_from_dict(cfg)
        self.__write_whole_Dicom_ct(voxel_materials)