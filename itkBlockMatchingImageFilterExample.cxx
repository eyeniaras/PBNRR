/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include <iostream>
#include "itkIndex.h"
#include "itkImage.h"
#include "itkRGBPixel.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionIterator.h"
#include "itkImageDuplicator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkLineIterator.h"
#include "itkMultiThreader.h"
#include "itkRegionOfInterestImageFilter.h"
#include "itkMaskFeaturePointSelectionFilter.h"
#include "itkBlockMatchingImageFilter.h"
#include "itkScalarToRGBColormapImageFilter.h"
#include "itkTranslationTransform.h"
#include "itkResampleImageFilter.h"
#include "GetPot.h"


int main( int argc, char * argv[] )
{
  if( argc < 2 )
    {
    std::cerr << "Usage: " << std::endl<< std::endl;
    std::cerr << " itkitkBlockMatchingImageFilterExample IniFile" << std::endl << std::endl;
    std::cerr << "   step one: find all feature points on a moving image" << std::endl;
    std::cerr << "             within a variance threshold              " << std::endl;
    std::cerr << "   step two: within search window on fixed image      " << std::endl;
    std::cerr << "             find block radius window center location " << std::endl;
    std::cerr << "             with the highest similarity to the moving" << std::endl;
    std::cerr << "             image feature point" << std::endl << std::endl;
    std::cerr << "   assumption: the search window on the fixed image   " << std::endl;
    std::cerr << "               is the feature point location in       " << std::endl;
    std::cerr << "               physical space +- the search radius    " << std::endl;
    std::cerr << "               ie, all point match pairs should be    " << std::endl;
    std::cerr << "               within the search radius neighborhood  " << std::endl;
    std::cerr << "               defined the the physical space location" << std::endl;
    std::cerr << "               of the feature point +- the search radius" << std::endl;
    std::cerr << "   assumption: the fixed and moving image should have " << std::endl;
    std::cerr << "               the same dimensions    " << std::endl<< std::endl;
    std::cerr << "   ...ie, rigid registration and resampling are probably needed" << std::endl << std::endl;
    return EXIT_FAILURE;
    }

  GetPot controlfile( argv[1] );

  const double selectFraction = controlfile("featureselection/selectionfraction",0.01);

  typedef float                          InputPixelType;
  typedef double                         NumpyArrayType;
  static const unsigned int Dimension = 3;
  typedef itk::Image< InputPixelType,  Dimension >  InputImageType;

  // set anisotropic match and search windows
  // ini file should of the form
  // [featureselection]
  // connectivityradius  = '7 7 6'
  // [blockmatch]
  // blockradius  = '3 3 2'
  // searchradius = '5 5 4'
  // 
  // Parameters used for FS and BM
  typedef InputImageType::SizeType RadiusType;
  RadiusType blockMatchRadius, featureBlockRadius, searchRadius, connectivityRadius;
  for (int icoord = 0; icoord < Dimension; icoord++)
   {
    blockMatchRadius.SetElement(  icoord, 
                             controlfile("blockmatch/blockradius" , 3, icoord) );
    searchRadius.SetElement( icoord,
                             controlfile("blockmatch/searchradius", 7, icoord) );
    connectivityRadius.SetElement(  icoord, 
                             controlfile("featureselection/connectivityradius" , 1, icoord) );
    featureBlockRadius.SetElement(  icoord, 
                             controlfile("featureselection/blockradius" , 3, icoord) );
   }
  // get the preprocess id
  // int PreProcessID = controlfile("image/preprocess",0) ;
  // Append Tag to Id output
  // std::ostringstream OutputFileTag;
  // OutputFileTag <<  "PreProcess" << PreProcessID ;
  // OutputFileTag <<  "BlockMatchRadius";
  // for (int icoord = 0; icoord < Dimension; icoord++) OutputFileTag <<  blockMatchRadius.GetElement(  icoord);
  // OutputFileTag <<  "SearchRadius";
  // for (int icoord = 0; icoord < Dimension; icoord++) OutputFileTag <<  searchRadius.GetElement(      icoord);
  // OutputFileTag <<  "ConnectRadius";
  // for (int icoord = 0; icoord < Dimension; icoord++) OutputFileTag <<  connectivityRadius.GetElement(icoord);
  // OutputFileTag <<  "FeatureRadius";
  // for (int icoord = 0; icoord < Dimension; icoord++) OutputFileTag <<  featureBlockRadius.GetElement(icoord);


  typedef itk::ImageFileReader< InputImageType >  ReaderType;

  // read in moving image first and identify the feature points
  ReaderType::Pointer readerFeature = ReaderType::New();
  std::string FeatureImageFileName( controlfile("image/featureimage","FeatureImageNotFound") ) ;
  readerFeature->SetFileName( FeatureImageFileName.c_str() );
  try
    {
    readerFeature->Update();
    }
  catch( itk::ExceptionObject & e )
    {
    std::cerr << "Error in reading the input image: " << e << std::endl;
    return EXIT_FAILURE;
    }
  InputImageType::Pointer FeatureImage = readerFeature->GetOutput() ;

  // Reduce region of interest by SEARCH_RADIUS
  typedef itk::RegionOfInterestImageFilter< InputImageType, InputImageType >  RegionOfInterestFilterType;

  RegionOfInterestFilterType::Pointer regionOfInterestFilter = RegionOfInterestFilterType::New();

  regionOfInterestFilter->SetInput( FeatureImage );

  RegionOfInterestFilterType::RegionType regionOfInterest = FeatureImage->GetLargestPossibleRegion();

  // set the starting point of the ROI
  RegionOfInterestFilterType::RegionType::IndexType regionOfInterestIndex = regionOfInterest.GetIndex();
  regionOfInterestIndex += searchRadius;
  regionOfInterest.SetIndex( regionOfInterestIndex );

  // the ROI size is the full size minus the radius on the beging
  //   and minus the radius on the end 
  RegionOfInterestFilterType::RegionType::SizeType regionOfInterestSize = regionOfInterest.GetSize();
  regionOfInterestSize -= searchRadius + searchRadius;
  regionOfInterest.SetSize( regionOfInterestSize );

  regionOfInterestFilter->SetRegionOfInterest( regionOfInterest );
  regionOfInterestFilter->Update();

  //regionOfInterestFilter->DebugOn();
  std::cout << "Region Of Interest: " << regionOfInterestFilter << std::endl;
  if ( regionOfInterestFilter->GetDebug() )
   {
     std::ostringstream ROIFileName;
     ROIFileName  << controlfile("image/output","./Output") << "ROI.mha" ;
     //Set up the writer
     typedef itk::ImageFileWriter< InputImageType >  WriterType;
     WriterType::Pointer writer = WriterType::New();

     writer->SetFileName( ROIFileName.str() );
     writer->SetInput( regionOfInterestFilter->GetOutput() );
     try
       {
       writer->Update();
       }
     catch( itk::ExceptionObject & e )
       {
       std::cerr << "Error in writing the output image:" << e << std::endl;
       return EXIT_FAILURE;
       }
   }

  // typedefs
  typedef itk::MaskFeaturePointSelectionFilter< InputImageType >  FeatureSelectionFilterType;
  typedef FeatureSelectionFilterType::FeaturePointsType           PointSetType;

  typedef FeatureSelectionFilterType::PointType       FeatureSelectionPointType;
  typedef FeatureSelectionFilterType::InputImageType  FeatureSelectionImageType;

  // Feature Selection
  FeatureSelectionFilterType::Pointer featureSelectionFilter = FeatureSelectionFilterType::New();

  featureSelectionFilter->SetInput( regionOfInterestFilter->GetOutput() );
  featureSelectionFilter->SetSelectFraction( selectFraction );
  featureSelectionFilter->SetBlockRadius( featureBlockRadius );
  featureSelectionFilter->SetConnectivityRadius( connectivityRadius );
  featureSelectionFilter->ComputeStructureTensorsOff();
  featureSelectionFilter->SetNonConnectivity((unsigned int)controlfile("featureselection/nonconnectivity",0));

  // check if user input mask image
  std::string FeatureMaskFileName( controlfile("image/featuremask","MovingMaskNotFound") ) ;
  if (FeatureMaskFileName.find("MovingMaskNotFound")!=std::string::npos)
   {
     std::cout << "No Moving Image Mask input... " << std::endl;
     std::cout << "No Moving Image Mask input... " << std::endl;
   }
  else
   {
    typedef short                                     MaskPixelType;
    typedef itk::Image< MaskPixelType,  Dimension >   MaskImageType;
    typedef itk::ImageFileReader< MaskImageType >     MaskReaderType;
    MaskReaderType::Pointer readerMovingMask = MaskReaderType::New();
    readerMovingMask->SetFileName( FeatureMaskFileName.c_str() );
    try
      {
        readerMovingMask->Update();
      }
    catch( itk::ExceptionObject & e )
      {
        std::cerr << "Error in reading the moving image mask image: " <<  FeatureMaskFileName << e << std::endl;
      }
    // set mask image
    featureSelectionFilter->SetMaskImage( readerMovingMask->GetOutput() );
   }
  // update and write selection points
  std::cout << "Feature Selection: " << featureSelectionFilter << std::endl;
  featureSelectionFilter->Update();
  // open file to write feature points
  std::ofstream      PhysicalFeatureFile; 
  std::ostringstream PhysicalFeatureFileName;
  PhysicalFeatureFileName << controlfile("image/output","./Output") <<  "PhysicalTargetPts.txt" ;
  PhysicalFeatureFile.open( PhysicalFeatureFileName.str().c_str()  );

  typedef itk::Point<float, Dimension >      ITKFloatPointType;
  typedef std::vector< ITKFloatPointType > STLFeaturePointType;
  const STLFeaturePointType &stlfeaturepoints = 
        featureSelectionFilter->GetOutput()->GetPoints()->CastToSTLConstContainer();
  const int NumFeaturePoints = stlfeaturepoints.size(); 
  // pass back to python as 1d array, reshape in numpy...
  std::vector< NumpyArrayType > StlFeaturePointsPassToCython;

  // write points
  for (STLFeaturePointType::const_iterator ptIt = stlfeaturepoints.begin(); 
                                           ptIt!= stlfeaturepoints.end()  ; ++ptIt) 
    {
    const ITKFloatPointType &featurepoint = *ptIt;
    for (int icoord = 0; icoord < Dimension; icoord++)
      StlFeaturePointsPassToCython.push_back(  featurepoint[icoord] );
    PhysicalFeatureFile << featurepoint[0]<<" "
                        << featurepoint[1]<<" "
                        << featurepoint[2]<< std::endl;
    }
  // close file
  PhysicalFeatureFile.close();

  // echo output points
  std::cout << "Wrote "<< NumFeaturePoints << " Feature Points..." << std::endl;

  //Set up the moving target reader
  ReaderType::Pointer readerMoving = ReaderType::New();
  readerMoving->SetFileName( controlfile("image/moving","./MovingNotFound") );
  try
    {
    readerMoving->Update();
    }
  catch( itk::ExceptionObject & e )
    {
    std::cerr << "Error in reading the input image: " << e << std::endl;
    return EXIT_FAILURE;
    }
  InputImageType::Pointer MovingImage = readerMoving->GetOutput() ;

  //Set up the fixed source reader
  ReaderType::Pointer readerFixed = ReaderType::New();
  readerFixed->SetFileName( controlfile("image/fixed","./FixedNotFound") );
  try
    {
    readerFixed->Update();
    }
  catch( itk::ExceptionObject & e )
    {
    std::cerr << "Error in reading the input image: " << e << std::endl;
    return EXIT_FAILURE;
    }


  typedef itk::BlockMatchingImageFilter< InputImageType >  BlockMatchingFilterType;
  BlockMatchingFilterType::Pointer blockMatchingFilter = BlockMatchingFilterType::New();

  // inputs (all required)
  InputImageType::Pointer Fixed_Image = readerFixed->GetOutput() ;
  blockMatchingFilter->SetFixedImage(  Fixed_Image );
  blockMatchingFilter->SetMovingImage( MovingImage );
  blockMatchingFilter->SetFeaturePoints( featureSelectionFilter->GetOutput() );

  if (Fixed_Image->GetSpacing() != MovingImage->GetSpacing() ) 
   {
    std::cerr << " image spacing not equal: " << std::endl;
   }

  // parameters (all optional)
  blockMatchingFilter->SetNumberOfThreads( controlfile("exec/threads" , 
       (int)  blockMatchingFilter->GetMultiThreader()->GetGlobalDefaultNumberOfThreads()
                                         )  );
  //blockMatchingFilter->DebugOn( );
  blockMatchingFilter->SetBlockRadius( blockMatchRadius);
  blockMatchingFilter->SetSearchRadius( searchRadius );

  std::cout << "Block matching: " << blockMatchingFilter << std::endl;
  try
    {
    blockMatchingFilter->Update();
    }
  catch ( itk::ExceptionObject &err )
    {
    std::cerr << err << std::endl;
    return EXIT_FAILURE;
    }

  // Exercise the following methods
  BlockMatchingFilterType::DisplacementsType * displacements = blockMatchingFilter->GetDisplacements();
  if( displacements == NULL )
    {
    std::cerr << "GetDisplacements() failed." << std::endl;
    return EXIT_FAILURE;
    }
  BlockMatchingFilterType::SimilaritiesType * similarities = blockMatchingFilter->GetSimilarities();
  if( similarities == NULL )
    {
    std::cerr << "GetSimilarities() failed." << std::endl;
    return EXIT_FAILURE;
    }

  // write txt files of block matching data
  //   source, target, displacements, similarity
  std::ofstream      DisplacementFile,    SimilarityFile;
  std::ostringstream DisplacementFileName,SimilarityFileName;

  DisplacementFileName << controlfile("image/output","./Output") <<  "Displacement.txt";
  SimilarityFileName   << controlfile("image/output","./Output") <<  "Similarity.txt"  ;

  // similarities iterator
  const std::vector< NumpyArrayType > &stlsimilarities = similarities->GetPointData()->CastToSTLConstContainer();
  const int NumSimilarityPoints = stlsimilarities.size(); 

  // displacement iterator
  typedef itk::Vector<float, Dimension >            ITKFloatVectorType;
  typedef std::vector< ITKFloatVectorType > STLDisplacementVectorType;
  const STLDisplacementVectorType &stldisplacements =  displacements->GetPointData()->CastToSTLConstContainer();
  const int NumDisplacementVec = stldisplacements.size(); 

  // error check
  if( NumDisplacementVec  != NumFeaturePoints 
             or
      NumFeaturePoints    != NumSimilarityPoints 
             or
      NumSimilarityPoints != NumDisplacementVec  
    )
    {
    std::cerr << "Number of points do not match !!! "           << std::endl;
    std::cerr << "NumFeaturePoints:    " << NumFeaturePoints    << std::endl;
    std::cerr << "NumSimilarityPoints: " << NumSimilarityPoints << std::endl;
    std::cerr << "NumDisplacementVec:  " << NumDisplacementVec  << std::endl;
    return EXIT_FAILURE;
    }

  // open file and write similarity
  SimilarityFile.open  ( SimilarityFileName.str().c_str()   );
  for (std::vector< NumpyArrayType >::const_iterator similItr = stlsimilarities.begin(); 
                                             similItr!= stlsimilarities.end()  ; ++similItr) 
    {
    SimilarityFile   << *similItr << std::endl;
    }
  SimilarityFile.close(); 


  // pass back to python as 1d array, reshape in numpy...
  std::vector< NumpyArrayType > StlDisplacementsPassToCython;
  // open file and write displacement
  DisplacementFile.open( DisplacementFileName.str().c_str() );
  for (STLDisplacementVectorType::const_iterator dplIt = stldisplacements.begin(); 
                                                 dplIt!= stldisplacements.end()  ; ++dplIt) 
    {
    const ITKFloatVectorType &displacementvalue = *dplIt;
    for (int icoord = 0; icoord < Dimension; icoord++)
      StlDisplacementsPassToCython.push_back(  displacementvalue[icoord] );
    DisplacementFile << displacementvalue[0]<<" "
                     << displacementvalue[1]<<" "
                     << displacementvalue[2]<< std::endl;
    }
  DisplacementFile.close();

  // typedefs
  typedef PointSetType::PointsContainer::ConstIterator                                   PointIteratorType;
  typedef BlockMatchingFilterType::DisplacementsType::PointDataContainer::ConstIterator  PointDataIteratorType;

  // write out block match indicies
  std::ofstream      FeatureIndexFile    ,FixedIndexFile    ,PhysSrcePtsFile    ; 
  std::ostringstream FeatureIndexFileName,FixedIndexFileName,PhysSrcePtsFileName;
  FeatureIndexFileName << controlfile("image/output","./Output") << "MovingTargetIndex.txt" ;
  FixedIndexFileName   << controlfile("image/output","./Output") << "Fixed_SourceIndex.txt" ;
  PhysSrcePtsFileName  << controlfile("image/output","./Output") << "PhysicalSourcePts.txt" ;

  PointIteratorType pointItr = featureSelectionFilter->GetOutput()->GetPoints()->Begin();
  PointIteratorType pointEnd = featureSelectionFilter->GetOutput()->GetPoints()->End();
  PointDataIteratorType displItr = displacements->GetPointData()->Begin();
  // open
  FeatureIndexFile.open(    FeatureIndexFileName.str().c_str()     );
  FixedIndexFile.open(        FixedIndexFileName.str().c_str()     );
  PhysSrcePtsFile.open(      PhysSrcePtsFileName.str().c_str()     );
  InputImageType::IndexType featureIndex;
  while ( pointItr != pointEnd )
    {
    if ( MovingImage->TransformPhysicalPointToIndex(pointItr.Value(), featureIndex) )
      {
      const ITKFloatPointType &physicalfixedpt = pointItr.Value() + displItr.Value();
      InputImageType::IndexType fixedIndex;
      Fixed_Image->TransformPhysicalPointToIndex( pointItr.Value() + displItr.Value(), fixedIndex );

      // write feature index
      FeatureIndexFile  << featureIndex[0]<<" "
                        << featureIndex[1]<<" "
                        << featureIndex[2]<< std::endl;

      // write fixed index 
      FixedIndexFile    << fixedIndex[0]<<" "
                        << fixedIndex[1]<<" "
                        << fixedIndex[2]<< std::endl;

      // write fixed physical location
      PhysSrcePtsFile   << physicalfixedpt[0]<<" "
                        << physicalfixedpt[1]<<" "
                        << physicalfixedpt[2]<< std::endl;
      } 
    pointItr++;
    displItr++;
    }
  // close file
  FeatureIndexFile.close();
  FixedIndexFile.close();
  PhysSrcePtsFile.close();

  // typedef itk::ImageDuplicator< InputImageType > DuplicatorType;
  // DuplicatorType::Pointer duplicator = DuplicatorType::New();
  // duplicator->SetInputImage(MovingImage);
  // duplicator->Update();
  // InputImageType::Pointer outputImage = duplicator->GetModifiableOutput();

  // create RGB copy of input image
  // TODO: Map Scalars Must be unchecked in paraview visualize the RGB image mapped from unsigned char
  // http://paraview.org/OnlineHelpCurrent/Display.html
  typedef itk::RGBPixel<unsigned char>  OutputPixelType;
  typedef itk::Image< OutputPixelType, Dimension >  OutputImageType;
  typedef itk::ScalarToRGBColormapImageFilter< InputImageType, OutputImageType > RGBFilterType;
  RGBFilterType::Pointer colormapMovingImageFilter = RGBFilterType::New();
  RGBFilterType::Pointer colormapFixed_ImageFilter = RGBFilterType::New();
  //colormapImageFilter->UseInputImageExtremaForScalingOff();

  // color maps
  colormapMovingImageFilter->SetColormap( RGBFilterType::Grey );
  colormapFixed_ImageFilter->SetColormap( RGBFilterType::Grey );

  // inputs
  colormapMovingImageFilter->SetInput( MovingImage  );
  colormapFixed_ImageFilter->SetInput( Fixed_Image  );
  try
    {
    colormapMovingImageFilter->Update( );
    colormapFixed_ImageFilter->Update( );
    }
  catch ( itk::ExceptionObject &err )
    {
    std::cerr << err << std::endl;
    return EXIT_FAILURE;
    }
  // write after update to get values used
  std::cout << "Converted Block Match Pairs to RGB- Moving: " << colormapMovingImageFilter<< std::endl;
  std::cout << "Converted Block Match Pairs to RGB- Fixed: "  << colormapFixed_ImageFilter << std::endl;
  OutputImageType::Pointer outputMovingImage = colormapMovingImageFilter->GetOutput();
  OutputImageType::Pointer outputFixed_Image = colormapFixed_ImageFilter->GetOutput();

  // // reset to zero
  // OutputPixelType ZeroValue;
  // ZeroValue.SetRed(   0 );
  // ZeroValue.SetGreen( 0 );
  // ZeroValue.SetBlue(  0 );
  // outputImage->FillBuffer( ZeroValue );

  // Define colors to highlight the feature points identified in the output image
  // TODO: Map Scalars Must be unchecked in paraview visualize the RGB image mapped from unsigned char
  // http://paraview.org/OnlineHelpCurrent/Display.html
  OutputPixelType red;
  red.SetRed( 255 );
  red.SetGreen( 0 );
  red.SetBlue( 0 );

  OutputPixelType green;
  green.SetRed( 0 );
  green.SetGreen( 255 );
  green.SetBlue( 0 );

  OutputPixelType blue;
  blue.SetRed( 0 );
  blue.SetGreen( 0 );
  blue.SetBlue( 255 );

  // reset the iterators
  pointItr = featureSelectionFilter->GetOutput()->GetPoints()->Begin();
  displItr = displacements->GetPointData()->Begin();

  OutputImageType::IndexType MovingIndex, Fixed_Index;
  while ( pointItr != pointEnd )
    {
    if ( outputMovingImage->TransformPhysicalPointToIndex(pointItr.Value(), MovingIndex) )
      {
      outputFixed_Image->TransformPhysicalPointToIndex(pointItr.Value(), Fixed_Index) ;

      OutputImageType::IndexType MovingDispl, Fixed_Displ;
      outputMovingImage->TransformPhysicalPointToIndex( pointItr.Value() + displItr.Value(), MovingDispl );
      outputFixed_Image->TransformPhysicalPointToIndex( pointItr.Value() + displItr.Value(), Fixed_Displ );

      // draw line between old and new location of a point in blue
      itk::LineIterator< OutputImageType > lineMovingIter( outputMovingImage, MovingIndex, MovingDispl);
      for ( lineMovingIter.GoToBegin(); !lineMovingIter.IsAtEnd(); ++lineMovingIter )
        {
        lineMovingIter.Set( blue );
        }
      itk::LineIterator< OutputImageType > lineFixed_Iter( outputFixed_Image, Fixed_Index, Fixed_Displ);
      for ( lineFixed_Iter.GoToBegin(); !lineFixed_Iter.IsAtEnd(); ++lineFixed_Iter )
        {
        lineFixed_Iter.Set( blue );
        }

      // mark old location of a point in green
      outputMovingImage->SetPixel(MovingIndex, green);

      // mark new location of a point in red
      outputFixed_Image->SetPixel(Fixed_Displ, red);
      }
    pointItr++;
    displItr++;
    }
  

  //Set up the writer
  typedef itk::ImageFileWriter< OutputImageType >  WriterType;
  WriterType::Pointer MovingWriter = WriterType::New();
  WriterType::Pointer Fixed_Writer = WriterType::New();

  std::ofstream      MovingBlockMatchFile,Fixed_BlockMatchFile;
  std::ostringstream MovingBlockMatchFileName,Fixed_BlockMatchFileName;
  MovingBlockMatchFileName << controlfile("image/output","./Output")<<"MovingTarget" << ".mha" ;
  Fixed_BlockMatchFileName << controlfile("image/output","./Output")<<"Fixed_Source" << ".mha" ;
  MovingWriter->SetFileName( MovingBlockMatchFileName.str().c_str() );
  Fixed_Writer->SetFileName( Fixed_BlockMatchFileName.str().c_str() );
  MovingWriter->SetInput( outputMovingImage );
  Fixed_Writer->SetInput( outputFixed_Image );
  // write after update to get values used
  try
    {
    MovingWriter->Update();
    Fixed_Writer->Update();
    }
  catch( itk::ExceptionObject & e )
    {
    std::cerr << "Error in writing the output image:" << e << std::endl;
    return EXIT_FAILURE;
    }

  // TODO - landmark based transform
  //  typedef itk::Rigid2DTransform< double > Rigid2DTransformType;
  //   typedef itk::LandmarkBasedTransformInitializer< Rigid2DTransformType,
  // ImageType, ImageType > 
  //       LandmarkBasedTransformInitializerType;
  //  
  //   LandmarkBasedTransformInitializerType::Pointer
  // landmarkBasedTransformInitializer =
  //     LandmarkBasedTransformInitializerType::New();
  return EXIT_SUCCESS;
}

