/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfig.h"

#if HAS_GRAPHICAL_TFT

#include "tft_queue.h"
#include "tft.h"
#include "tft_image.h"

uint8_t TFT_Queue::queue[];
uint8_t *TFT_Queue::end_of_queue = queue;
uint8_t *TFT_Queue::current_task = nullptr;
uint8_t *TFT_Queue::last_task = nullptr;
uint8_t *TFT_Queue::last_parameter = nullptr;

bool TFT_Queue::failed = false;
bool TFT_Queue::failure_pending = false;

void TFT_Queue::clear_storage() {
  end_of_queue = queue;
  current_task = nullptr;
  last_task = nullptr;
  last_parameter = nullptr;
  *end_of_queue = TASK_END_OF_QUEUE;
}

void TFT_Queue::reset() {
  tft.abort();
  clear_storage();
  failed = failure_pending = false;
}

void TFT_Queue::reject() {
  // Latch once. Keep accepted tasks and their DMA data alive until drained.
  if (failed) return;
  failed = failure_pending = true;
  finish_sketch();
}

bool TFT_Queue::reserve(size_t sizeNeeded) {
  if (failed) return false;
  const size_t used = end_of_queue - queue;
  // Every allocation leaves one byte for TASK_END_OF_QUEUE, even at capacity.
  if (used >= sizeof(queue) || sizeNeeded > sizeof(queue) - used - 1) {
    reject();
    return false;
  }
  return true;
}

bool TFT_Queue::reserve_canvas(size_t sizeNeeded) {
  if (failed) return false;
  // A fill, a sealed canvas or a missing parent cannot accept parameters.
  if (!last_task || ((queueTask_t *)last_task)->type != TASK_CANVAS
      || ((queueTask_t *)last_task)->state != TASK_STATE_SKETCH) {
    reject();
    return false;
  }
  return reserve(sizeNeeded);
}

void TFT_Queue::show_failure() {
  // Called only after the consumer and DMA have released all accepted tasks.
  static uint8_t message[] = "LCD: cola llena / queue full";
  static uint8_t action[] = "Volver / Back";
  static_assert(sizeof(queueTask_t) + sizeof(parametersCanvas_t)
    + sizeof(parametersCanvasBackground_t) + 2 * sizeof(parametersCanvasText_t)
    + sizeof(message) + sizeof(action) + 1 <= TFT_QUEUE_SIZE, "TFT queue cannot hold its error message");
  failure_pending = false;
  failed = false; // Only this bounded diagnostic may allocate after rejection.
  canvas(0, 0, TFT_WIDTH, TFT_HEIGHT);
  set_background(COLOR_BACKGROUND);
  add_text(8, 8, COLOR_WHITE, message, TFT_WIDTH - 16);
  add_text(8, 32, COLOR_WHITE, action, TFT_WIDTH - 16);
  finish_sketch();
  failed = true;  // An explicit reset / screen change is needed before retrying.
}

void TFT_Queue::async() {
  // Never recycle task memory or the pixel buffer while DMA owns them.
  if (tft.is_busy()) return;
  if (current_task && *current_task != TASK_END_OF_QUEUE
      && ((queueTask_t *)current_task)->state == TASK_STATE_COMPLETED)
    current_task = ((queueTask_t *)current_task)->nextTask;

  if (current_task && *current_task == TASK_END_OF_QUEUE) clear_storage();
  if (!current_task) {
    if (failure_pending) show_failure();
    else return;
  }

  finish_sketch();
  queueTask_t *task = (queueTask_t *)current_task;
  switch (task->type) {
    case TASK_FILL:   fill(task);   break;
    case TASK_CANVAS: canvas(task); break;
    default: reject();             break;
  }
}

void TFT_Queue::finish_sketch() {
  if (!last_task) return;
  queueTask_t *task = (queueTask_t *)last_task;

  if (task->state == TASK_STATE_SKETCH) {
    *end_of_queue = TASK_END_OF_QUEUE;
    task->nextTask = end_of_queue;
    task->state = TASK_STATE_READY;

    if (!current_task) current_task = (uint8_t *)task;
  }
}

void TFT_Queue::fill(queueTask_t *task) {
  uint16_t count;
  parametersFill_t *task_parameters = (parametersFill_t *)(((uint8_t *)task) + sizeof(queueTask_t));

  if (task->state == TASK_STATE_READY) {
    tft.set_window(task_parameters->x, task_parameters->y, task_parameters->x + task_parameters->width - 1, task_parameters->y + task_parameters->height - 1);
    task->state = TASK_STATE_IN_PROGRESS;
  }

  if (task_parameters->count > 65535) {
    count = 65535;
    task_parameters->count -= 65535;
  }
  else {
    count = task_parameters->count;
    task_parameters->count = 0;
    task->state = TASK_STATE_COMPLETED;
  }

  tft.write_multiple(task_parameters->color, count);
}

void TFT_Queue::canvas(queueTask_t *task) {
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)task) + sizeof(queueTask_t));

  uint32_t i;
  uint8_t *item = ((uint8_t *)task_parameters) + sizeof(parametersCanvas_t);

  if (task->state == TASK_STATE_READY) {
    task->state = TASK_STATE_IN_PROGRESS;
    Canvas.New(task_parameters->x, task_parameters->y, task_parameters->width, task_parameters->height);
  }
  Canvas.Continue();

  for (i = 0; i < task_parameters->count; i++) {
    switch (*item) {
      case CANVAS_SET_BACKGROUND:
        Canvas.SetBackground(((parametersCanvasBackground_t *)item)->color);
        break;
      case CANVAS_ADD_TEXT:
        Canvas.AddText(((parametersCanvasText_t *)item)->x, ((parametersCanvasText_t *)item)->y, ((parametersCanvasText_t *)item)->color, item + sizeof(parametersCanvasText_t), ((parametersCanvasText_t *)item)->maxWidth);
        break;

      case CANVAS_ADD_IMAGE:
        MarlinImage image;
        uint16_t *colors;

        image = ((parametersCanvasImage_t *)item)->image;
        colors = (uint16_t *)(item + sizeof(parametersCanvasImage_t));
        Canvas.AddImage(((parametersCanvasImage_t *)item)->x, ((parametersCanvasImage_t *)item)->y, image, colors);
        break;

      case CANVAS_ADD_BAR:
        Canvas.AddBar(((parametersCanvasBar_t *)item)->x, ((parametersCanvasBar_t *)item)->y, ((parametersCanvasBar_t *)item)->width, ((parametersCanvasBar_t *)item)->height, ((parametersCanvasBar_t *)item)->color);
        break;
#if ENABLED(PANTALLA_NUEVA)
      case CANVAS_ADD_TEXT_BIG:
        Canvas.AddTextBig(((parametersCanvasText_t *)item)->x, ((parametersCanvasText_t *)item)->y, ((parametersCanvasText_t *)item)->color, item + sizeof(parametersCanvasText_t));
        break;
      case CANVAS_ADD_ROUND:
        Canvas.AddRound(((parametersCanvasRound_t *)item)->x, ((parametersCanvasRound_t *)item)->y, ((parametersCanvasRound_t *)item)->width, ((parametersCanvasRound_t *)item)->height, ((parametersCanvasRound_t *)item)->radius, ((parametersCanvasRound_t *)item)->color);
        break;
      case CANVAS_ADD_PIXELS:
        Canvas.AddPixels(((parametersCanvasPixels_t *)item)->x, ((parametersCanvasPixels_t *)item)->y, ((parametersCanvasPixels_t *)item)->width, ((parametersCanvasPixels_t *)item)->height, ((parametersCanvasPixels_t *)item)->data, ((parametersCanvasPixels_t *)item)->dimRows);
        break;
#endif
      case CANVAS_ADD_RECTANGLE:
        Canvas.AddRectangle(((parametersCanvasRectangle_t *)item)->x, ((parametersCanvasRectangle_t *)item)->y, ((parametersCanvasRectangle_t *)item)->width, ((parametersCanvasRectangle_t *)item)->height, ((parametersCanvasRectangle_t *)item)->color);
        break;
    }
    item = ((parametersCanvasBackground_t *)item)->nextParameter;
  }

  if (Canvas.ToScreen()) task->state = TASK_STATE_COMPLETED;
}

void TFT_Queue::fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  finish_sketch();
  if (!width || !height) return;
  if (!reserve(sizeof(queueTask_t) + sizeof(parametersFill_t))) return;

  queueTask_t *task = (queueTask_t *)end_of_queue;
  last_task = (uint8_t *)task;

  end_of_queue += sizeof(queueTask_t);
  parametersFill_t *task_parameters = (parametersFill_t *)end_of_queue;
  end_of_queue += sizeof(parametersFill_t);

  last_parameter = end_of_queue;
  task_parameters->x = x;
  task_parameters->y = y;
  task_parameters->width = width;
  task_parameters->height = height;
  task_parameters->color = ENDIAN_COLOR(color);
  task_parameters->count = uint32_t(width) * height;

  *end_of_queue = TASK_END_OF_QUEUE;
  task->nextTask = end_of_queue;
  task->state = TASK_STATE_READY;
  task->type = TASK_FILL;

  if (!current_task) current_task = (uint8_t *)task;
}

void TFT_Queue::canvas(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  finish_sketch();
  if (!width || !height || width > TFT_BUFFER_SIZE) { reject(); return; }
  if (!reserve(sizeof(queueTask_t) + sizeof(parametersCanvas_t))) return;

  queueTask_t *task = (queueTask_t *)end_of_queue;
  last_task = (uint8_t *) task;

  task->state = TASK_STATE_SKETCH;
  task->type = TASK_CANVAS;
  task->nextTask = nullptr;

  end_of_queue += sizeof(queueTask_t);
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)end_of_queue;
  end_of_queue += sizeof(parametersCanvas_t);

  last_parameter = end_of_queue;
  task_parameters->x = x;
  task_parameters->y = y;
  task_parameters->width = width;
  task_parameters->height = height;
  task_parameters->count = 0;

  if (!current_task) current_task = (uint8_t *)task;
}

void TFT_Queue::set_background(uint16_t color) {
  if (!reserve_canvas(sizeof(parametersCanvasBackground_t))) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)last_task) + sizeof(queueTask_t));
  parametersCanvasBackground_t *parameters = (parametersCanvasBackground_t *)end_of_queue;
  last_parameter = end_of_queue;

  parameters->type = CANVAS_SET_BACKGROUND;
  parameters->color = ENDIAN_COLOR(color);

  end_of_queue += sizeof(parametersCanvasBackground_t);
  task_parameters->count++;
  parameters->nextParameter = end_of_queue;
}

void TFT_Queue::add_text(uint16_t x, uint16_t y, uint16_t color, uint8_t *string, uint16_t maxWidth) {
  if (failed) return;
  if (!string) { reject(); return; }
  size_t length = 0;
  while (length < sizeof(queue) && string[length]) ++length;
  if (length == sizeof(queue) || length >= 65535U) { reject(); return; }
  if (!reserve_canvas(sizeof(parametersCanvasText_t) + length + 1)) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)last_task) + sizeof(queueTask_t));
  parametersCanvasText_t *parameters = (parametersCanvasText_t *)end_of_queue;
  last_parameter = end_of_queue;

  uint8_t *pointer = string;

  parameters->type = CANVAS_ADD_TEXT;
  parameters->x = x;
  parameters->y = y;
  parameters->color = ENDIAN_COLOR(color);
  parameters->stringLength = 0;
  parameters->maxWidth = maxWidth;

  end_of_queue += sizeof(parametersCanvasText_t);

  // maxWidth is a pixel limit; storage always contains the complete string.
  while ((*(end_of_queue++) = *pointer++) != 0x00);

  parameters->nextParameter = end_of_queue;
  parameters->stringLength = uint16_t(length + 1);
  task_parameters->count++;
}

void TFT_Queue::add_image(int16_t x, int16_t y, MarlinImage image, uint16_t *colors) {
  if (uint32_t(image) >= uint32_t(imgCount)) { reject(); return; }
  const colorMode_t color_mode = Images[image].colorMode;
  uint8_t color_count;
  switch (color_mode) {
    case HIGHCOLOR:  color_count = 0;  break;
    case GREYSCALE1: color_count = 1;  break;
    case GREYSCALE2: color_count = 3;  break;
    case GREYSCALE4: color_count = 15; break;
    default: reject(); return;
  }
  if (color_count && !colors) { reject(); return; }
  if (!reserve_canvas(sizeof(parametersCanvasImage_t) + color_count * sizeof(uint16_t))) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(last_task + sizeof(queueTask_t));
  parametersCanvasImage_t *parameters = (parametersCanvasImage_t *)end_of_queue;
  last_parameter = end_of_queue;
  parameters->type = CANVAS_ADD_IMAGE;
  parameters->x = x;
  parameters->y = y;
  parameters->image = image;
  end_of_queue += sizeof(parametersCanvasImage_t);
  uint16_t *color = (uint16_t *)end_of_queue;
  while (color_count--) {
    const uint16_t tmp = *colors++;
    *color++ = ENDIAN_COLOR(tmp);
  }
  end_of_queue = (uint8_t *)color;
  parameters->nextParameter = end_of_queue;
  task_parameters->count++;
}

#if ENABLED(PANTALLA_NUEVA)

// Texto a doble tamano: reutiliza add_text y solo cambia el tipo del elemento.
void TFT_Queue::add_text_big(uint16_t x, uint16_t y, uint16_t color, uint8_t *string) {
  add_text(x, y, color, string, 0);
  if (!failed && last_parameter)
    ((parametersCanvasText_t *)last_parameter)->type = CANVAS_ADD_TEXT_BIG;
}

void TFT_Queue::add_round(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t radius, uint16_t color) {
  if (!reserve_canvas(sizeof(parametersCanvasRound_t))) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)last_task) + sizeof(queueTask_t));
  parametersCanvasRound_t *parameters = (parametersCanvasRound_t *)end_of_queue;
  last_parameter = end_of_queue;

  parameters->type = CANVAS_ADD_ROUND;
  parameters->x = x;
  parameters->y = y;
  parameters->width = width;
  parameters->height = height;
  parameters->radius = radius;
  parameters->color = ENDIAN_COLOR(color);

  end_of_queue += sizeof(parametersCanvasRound_t);
  task_parameters->count++;
  parameters->nextParameter = end_of_queue;
}

void TFT_Queue::add_pixels(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data, uint16_t dimRows) {
  if (!reserve_canvas(sizeof(parametersCanvasPixels_t))) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)last_task) + sizeof(queueTask_t));
  parametersCanvasPixels_t *parameters = (parametersCanvasPixels_t *)end_of_queue;
  last_parameter = end_of_queue;

  parameters->type = CANVAS_ADD_PIXELS;
  parameters->x = x;
  parameters->y = y;
  parameters->width = width;
  parameters->height = height;
  parameters->dimRows = dimRows;
  parameters->data = data;

  end_of_queue += sizeof(parametersCanvasPixels_t);
  task_parameters->count++;
  parameters->nextParameter = end_of_queue;
}

#endif // PANTALLA_NUEVA
uint16_t gradient(uint16_t colorA, uint16_t colorB, uint16_t factor) {
  uint16_t red, green, blue;

  red   = (  RED(colorA) * factor +   RED(colorB) * (256 - factor)) >> 8;
  green = (GREEN(colorA) * factor + GREEN(colorB) * (256 - factor)) >> 8;
  blue  = ( BLUE(colorA) * factor +  BLUE(colorB) * (256 - factor)) >> 8;

  return RGB(red, green, blue);
}

void TFT_Queue::add_image(int16_t x, int16_t y, MarlinImage image, uint16_t color_main, uint16_t color_background, uint16_t color_shadow) {
  if (uint32_t(image) >= uint32_t(imgCount)) { reject(); return; }
  uint16_t colors[16];
  colorMode_t color_mode = Images[image].colorMode;
  uint16_t i;

  switch (color_mode) {
    case GREYSCALE1:
      colors[1] = color_main;
      break;
    case GREYSCALE2:
      for (i = 1; i < 4; i++)
        colors[i] = gradient(color_main, color_background, (i << 8) / 3);
      break;
    case GREYSCALE4:
      for (i = 1; i < 8; i++)
        colors[i] = gradient(color_background, color_shadow, i << 5);
      for (i = 8; i < 16; i++)
        colors[i] = gradient(color_main, color_background, ((i - 8) << 8) / 7);
      break;
    default:
      break;
  }

  add_image(x, y, image, colors + 1);
}

void TFT_Queue::add_bar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (!reserve_canvas(sizeof(parametersCanvasBar_t))) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)last_task) + sizeof(queueTask_t));
  parametersCanvasBar_t *parameters = (parametersCanvasBar_t *)end_of_queue;
  last_parameter = end_of_queue;

  parameters->type = CANVAS_ADD_BAR;
  parameters->x = x;
  parameters->y = y;
  parameters->width = width;
  parameters->height = height;
  parameters->color = ENDIAN_COLOR(color);

  end_of_queue += sizeof(parametersCanvasBar_t);
  task_parameters->count++;
  parameters->nextParameter = end_of_queue;
}

void TFT_Queue::add_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (!reserve_canvas(sizeof(parametersCanvasRectangle_t))) return;
  parametersCanvas_t *task_parameters = (parametersCanvas_t *)(((uint8_t *)last_task) + sizeof(queueTask_t));
  parametersCanvasRectangle_t *parameters = (parametersCanvasRectangle_t *)end_of_queue;
  last_parameter = end_of_queue;

  parameters->type = CANVAS_ADD_RECTANGLE;
  parameters->x = x;
  parameters->y = y;
  parameters->width = width;
  parameters->height = height;
  parameters->color = ENDIAN_COLOR(color);

  end_of_queue += sizeof(parametersCanvasRectangle_t);
  task_parameters->count++;
  parameters->nextParameter = end_of_queue;
}

#endif // HAS_GRAPHICAL_TFT
