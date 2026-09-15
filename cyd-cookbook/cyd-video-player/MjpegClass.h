/*******************************************************************************
 * JPEGDEC Wrapper Class
 *
 * Dependent libraries:
 * JPEGDEC: https://github.com/bitbank2/JPEGDEC.git
 ******************************************************************************/
#ifndef _MJPEGCLASS_H_
#define _MJPEGCLASS_H_

#define READ_BUFFER_SIZE 8192  // Increased from 1024 for better SD card throughput
#define MAXOUTPUTSIZE (MAX_BUFFERED_PIXELS / 16 / 16)

/* Wio Terminal */
#if defined(ARDUINO_ARCH_SAMD) && defined(SEEED_GROVE_UI_WIRELESS)
#include <Seeed_FS.h>
#elif defined(ESP32) || defined(ESP8266)
#include <FS.h>
#else
#include <SD.h>
#endif

#include <JPEGDEC.h>

class MjpegClass
{
public:
  int getWidth() const { return _jpgWidth; }
  int getHeight() const { return _jpgHeight; }
  int getScale() const { return _scale; }      // 0, 1⁄2, 1⁄4, 1⁄8  (JPEG_SCALE_x)

  bool setup(
      Stream *input, uint8_t *mjpeg_buf, JPEG_DRAW_CALLBACK *pfnDraw, bool useBigEndian,
      int x, int y, int widthLimit, int heightLimit)
  {
    _input = input;
    _mjpeg_buf = mjpeg_buf;
    _pfnDraw = pfnDraw;
    _useBigEndian = useBigEndian;
    _x = x;
    _y = y;
    _widthLimit = widthLimit;
    _heightLimit = heightLimit;
    _inputindex = 0;
    _scale = -1; // recompute scale and centering for each video

    if (!_read_buf)
    {
      _read_buf = (uint8_t *)malloc(READ_BUFFER_SIZE);
    }

    if (!_read_buf)
    {
      return false;
    }

    return true;
  }

  bool readMjpegBuf()
  {
    return readMjpegBufTo(_mjpeg_buf, &_mjpeg_buf_offset);
  }

  // Read MJPEG frame into a specified target buffer (for dual-core operation)
  bool readMjpegBufTo(uint8_t *targetBuf, int32_t *outLen)
  {
  retry: // an oversized frame is skipped and the next one read
    bool overflow = false;
    if (_inputindex == 0)
    {
      _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
      _inputindex += _buf_read;
    }
    uint32_t targetOffset = 0;
    int i = 0;
    bool found_FFD8 = false;
    while ((_buf_read > 0) && (!found_FFD8))
    {
      i = 0;
      while ((i + 1 < _buf_read) && (!found_FFD8))
      {
        if ((_read_buf[i] == 0xFF) && (_read_buf[i + 1] == 0xD8)) // JPEG header
        {
          // Serial.printf("Found FFD8 at: %d.\n", i);
          found_FFD8 = true;
        }
        ++i;
      }
      if (found_FFD8)
      {
        --i;
      }
      else
      {
        // keep a trailing 0xFF so a header split across two reads is still found
        int keep = (_buf_read > 0 && _read_buf[_buf_read - 1] == 0xFF) ? 1 : 0;
        if (keep)
          _read_buf[0] = 0xFF;
        int32_t n = _input->readBytes(_read_buf + keep, READ_BUFFER_SIZE - keep);
        _buf_read = (n > 0) ? n + keep : 0;
        vTaskDelay(1); // scanning non-JPEG data (e.g. AVI chunks), keep the watchdog fed
      }
    }
    uint8_t *_p = _read_buf + i;
    _buf_read -= i;
    bool found_FFD9 = false;
    if (_buf_read > 0)
    {
      i = 3;
      while ((_buf_read > 0) && (!found_FFD9))
      {
        if ((targetOffset > 0) && !overflow && (targetBuf[targetOffset - 1] == 0xFF) && (_p[0] == 0xD9)) // JPEG trailer
        {
          // Serial.printf("Found FFD9 at: %d.\n", i);
          found_FFD9 = true;
        }
        else
        {
          while ((i + 1 < _buf_read) && (!found_FFD9))
          {
            if ((_p[i] == 0xFF) && (_p[i + 1] == 0xD9)) // JPEG trailer
            {
              found_FFD9 = true;
              ++i;
            }
            ++i;
          }
        }

        // Serial.printf("i: %d\n", i);
        if (targetOffset + i > MJPEG_BUF_SIZE)
        {
          overflow = true;
          vTaskDelay(1); // corrupt data can run for MBs without a trailer, keep the watchdog fed
        }
        else
          memcpy(targetBuf + targetOffset, _p, i);
        targetOffset += i;
        size_t o = _buf_read - i;
        if (o > 0)
        {
          // Serial.printf("o: %d\n", o);
          memmove(_read_buf, _p + i, o); // regions can overlap
          int32_t n = _input->readBytes(_read_buf + o, READ_BUFFER_SIZE - o);
          _p = _read_buf;
          _inputindex += n;
          _buf_read = (n > 0 || i > 0) ? o + n : 0; // no progress and no more input: stop
          // Serial.printf("_buf_read: %d\n", _buf_read);
        }
        else
        {
          _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
          _p = _read_buf;
          _inputindex += _buf_read;
        }
        i = 0;
      }
      if (found_FFD9 && overflow)
      {
        Serial.printf("Frame of %u bytes > MJPEG_BUF_SIZE, skipped\n", targetOffset);
        vTaskDelay(1); // let IDLE run, or the task watchdog fires while skipping many frames
        goto retry;
      }
      if (found_FFD9)
      {
        *outLen = targetOffset;
        return true;
      }
    }

    *outLen = 0;
    return false;
  }

  bool drawJpg()
  {
    return drawJpgFrom(_mjpeg_buf, _mjpeg_buf_offset);
  }

  // Draw JPEG from a specified buffer (for dual-core operation)
  bool drawJpgFrom(uint8_t *srcBuf, int32_t srcLen)
  {
    _remain = srcLen;
    _jpeg.openRAM(srcBuf, _remain, _pfnDraw);
    if (_scale == -1)
    {
      // scale to fit height
      int iMaxMCUs;
      _jpgWidth = _jpeg.getWidth();
      _jpgHeight = _jpeg.getHeight();
      float ratio = (float)_jpgHeight / _heightLimit;
      if (ratio <= 1)
      {
        _scale = 0;
        iMaxMCUs = _widthLimit / 16;
      }
      else if (ratio <= 2)
      {
        _scale = JPEG_SCALE_HALF;
        iMaxMCUs = _widthLimit / 8;
        _jpgWidth /= 2;
        _jpgHeight /= 2;
      }
      else if (ratio <= 4)
      {
        _scale = JPEG_SCALE_QUARTER;
        iMaxMCUs = _widthLimit / 4;
        _jpgWidth /= 4;
        _jpgHeight /= 4;
      }
      else
      {
        _scale = JPEG_SCALE_EIGHTH;
        iMaxMCUs = _widthLimit / 2;
        _jpgWidth /= 8;
        _jpgHeight /= 8;
      }
      _maxMCUs = iMaxMCUs;
      _x = (_jpgWidth > _widthLimit) ? 0 : ((_widthLimit - _jpgWidth) / 2);
      _y = (_jpgHeight > _heightLimit) ? 0 : ((_heightLimit - _jpgHeight) / 2);
    }
    _jpeg.setMaxOutputSize(_maxMCUs); // openRAM resets it
    if (_useBigEndian)
    {
      _jpeg.setPixelType(RGB565_BIG_ENDIAN);
    }
    // center the (scaled) image on the display
    _jpeg.decode(_x, _y, _scale);
    _jpeg.close();

    return true;
  }

private:
  Stream *_input;
  uint8_t *_mjpeg_buf;
  JPEG_DRAW_CALLBACK *_pfnDraw;
  bool _useBigEndian;
  int _x;
  int _y;
  int _widthLimit;
  int _heightLimit;
  int _jpgWidth;
  int _jpgHeight;

  uint8_t *_read_buf;
  int32_t _mjpeg_buf_offset = 0;

  JPEGDEC _jpeg;
  int _scale = -1;
  int _maxMCUs = 1000;

  int32_t _inputindex = 0;
  int32_t _buf_read;
  int32_t _remain = 0;
};

#endif // _MJPEGCLASS_H_
