// Copyright(C) 2023 InfiniFlow, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

module;

#include <string>
import stl;
import query_context;
import parser;
import operator_state;
import physical_operator_type;
import third_party;
import fragment_data;
import data_block;

import infinity_exception;
import logger;

module physical_sink;

namespace infinity {

void PhysicalSink::Init() {}

bool PhysicalSink::Execute(QueryContext *, OperatorState *) { return true; }

bool PhysicalSink::Execute(QueryContext *, SinkState *sink_state) {
    switch (sink_state->state_type_) {
        case SinkStateType::kInvalid: {
            Error<ExecutorException>("Invalid sinker type");
            break;
        }
        case SinkStateType::kMaterialize: {
            // Output general output
            auto *materialize_sink_state = static_cast<MaterializeSinkState *>(sink_state);
            FillSinkStateFromLastOperatorState(materialize_sink_state, materialize_sink_state->prev_op_state_);
            break;
        }
        case SinkStateType::kResult: {
            // Output result
            auto *result_sink_state = static_cast<ResultSinkState *>(sink_state);
            FillSinkStateFromLastOperatorState(result_sink_state, result_sink_state->prev_op_state_);
            break;
        }
        case SinkStateType::kMessage: {
            // Output message
            auto *message_sink_state = static_cast<MessageSinkState *>(sink_state);
            FillSinkStateFromLastOperatorState(message_sink_state, message_sink_state->prev_op_state_);
            break;
        }
        case SinkStateType::kSummary: {
            // Output summary
            auto *summary_sink_state = static_cast<SummarySinkState *>(sink_state);
            FillSinkStateFromLastOperatorState(summary_sink_state, summary_sink_state->prev_op_state_);
            break;
        }
        case SinkStateType::kQueue: {
            QueueSinkState *queue_sink_state = static_cast<QueueSinkState *>(sink_state);
            FillSinkStateFromLastOperatorState(queue_sink_state, queue_sink_state->prev_op_state_);
            break;
        }
    }
    return true;
}

void PhysicalSink::FillSinkStateFromLastOperatorState(MaterializeSinkState *materialize_sink_state, OperatorState *task_op_state) {
    switch (task_op_state->operator_type_) {
        case PhysicalOperatorType::kInvalid: {
            Error<ExecutorException>("Invalid operator");
        }
        case PhysicalOperatorType::kShow: {
            ShowOperatorState *show_output_state = static_cast<ShowOperatorState *>(task_op_state);
            for (auto &data_block : show_output_state->output_) {
                materialize_sink_state->data_block_array_.emplace_back(Move(data_block));
            }
            show_output_state->output_.clear();
            break;
        }
        case PhysicalOperatorType::kExplain: {
            ExplainOperatorState *explain_output_state = static_cast<ExplainOperatorState *>(task_op_state);
            if (explain_output_state->data_block_array_.empty()) {
                Error<ExecutorException>("Empty explain output");
            }

            for (auto &data_block : explain_output_state->data_block_array_) {
                materialize_sink_state->data_block_array_.emplace_back(Move(data_block));
            }
            explain_output_state->data_block_array_.clear();
            break;
        }
        case PhysicalOperatorType::kProjection: {
            ProjectionOperatorState *projection_output_state = static_cast<ProjectionOperatorState *>(task_op_state);
            if (projection_output_state->data_block_array_.empty()) {
                if(materialize_sink_state->Error()) {
                    materialize_sink_state->empty_result_ = true;
                } else {
                    Error<ExecutorException>("Empty projection output");
                }
            } else {
                for (auto &data_block : projection_output_state->data_block_array_) {
                    materialize_sink_state->data_block_array_.emplace_back(Move(data_block));
                }
                projection_output_state->data_block_array_.clear();
            }
            break;
        }
        case PhysicalOperatorType::kSort: {
            SortOperatorState *sort_output_state = static_cast<SortOperatorState *>(task_op_state);
            if (sort_output_state->data_block_array_.empty()) {
                if(materialize_sink_state->Error()) {
                    materialize_sink_state->empty_result_ = true;
                } else {
                    Error<ExecutorException>("Empty sort output");
                }
            } else {
                for (auto &data_block : sort_output_state->data_block_array_) {
                    materialize_sink_state->data_block_array_.emplace_back(Move(data_block));
                }
                sort_output_state->data_block_array_.clear();
            }
            break;
        }
        case PhysicalOperatorType::kAggregate: {
            AggregateOperatorState *agg_output_state = static_cast<AggregateOperatorState *>(task_op_state);
            if (agg_output_state->data_block_array_.empty()) {
                if(materialize_sink_state->Error()) {
                    materialize_sink_state->empty_result_ = true;
                } else {
                    Error<ExecutorException>("Empty sort output");
                }
            } else {
                materialize_sink_state->data_block_array_ = Move(agg_output_state->data_block_array_);
            }
            break;
        }
        default: {
            Error<NotImplementException>(Format("{} isn't supported here.", PhysicalOperatorToString(task_op_state->operator_type_)));
        }
    }
}

void PhysicalSink::FillSinkStateFromLastOperatorState(SummarySinkState *summary_sink_state, OperatorState *task_operator_state) {
    switch (task_operator_state->operator_type_) {
        case PhysicalOperatorType::kInvalid: {
            Error<ExecutorException>("Invalid operator");
            break;
        }
        case PhysicalOperatorType::kDelete: {
            DeleteOperatorState *delete_operator_state = static_cast<DeleteOperatorState *>(task_operator_state);
            summary_sink_state->count_ = delete_operator_state->count_;
            summary_sink_state->sum_ = delete_operator_state->sum_;
            break;
        }
        case PhysicalOperatorType::kUpdate: {
            UpdateOperatorState *update_operator_state = static_cast<UpdateOperatorState *>(task_operator_state);
            summary_sink_state->count_ = update_operator_state->count_;
            summary_sink_state->sum_ = update_operator_state->sum_;
            break;
        }
        default: {
            Error<ExecutorException>(Format("{} isn't supported here.", PhysicalOperatorToString(task_operator_state->operator_type_)));
        }
    }
}

void PhysicalSink::FillSinkStateFromLastOperatorState(ResultSinkState *result_sink_state, OperatorState *task_operator_state) {
    switch (task_operator_state->operator_type_) {

        case PhysicalOperatorType::kInvalid: {
            Error<ExecutorException>("Invalid operator");
        }
        case PhysicalOperatorType::kCreateTable: {
            auto *output_state = static_cast<CreateTableOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kCreateIndex: {
            auto *output_state = static_cast<CreateIndexOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>()),
                };
            }
            break;
        }
        case PhysicalOperatorType::kCreateCollection: {
            auto *output_state = static_cast<CreateCollectionOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kCreateDatabase: {
            auto *output_state = static_cast<CreateDatabaseOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kCreateView: {
            auto *output_state = static_cast<CreateViewOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kDropTable: {
            auto *output_state = static_cast<DropTableOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kDropIndex: {
            auto *output_state = static_cast<DropIndexOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>()),
                };
            }
            break;
        }
        case PhysicalOperatorType::kDropCollection: {
            auto *output_state = static_cast<DropCollectionOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kDropDatabase: {
            auto *output_state = static_cast<DropDatabaseOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kDropView: {
            auto *output_state = static_cast<DropViewOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kCommand: {
            auto *output_state = static_cast<CommandOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kFlush: {
            auto *output_state = static_cast<FlushOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kOptimize: {
            auto *output_state = static_cast<OptimizeOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
            } else {
                result_sink_state->result_def_ = {
                    MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>())};
            }
            break;
        }
        case PhysicalOperatorType::kCreateIndexFinish: {
            auto *output_state = static_cast<CreateIndexFinishOperatorState *>(task_operator_state);
            if (output_state->error_message_.get() != nullptr) {
                result_sink_state->error_message_ = Move(output_state->error_message_);
                break;
            }
            result_sink_state->result_def_ = {
                MakeShared<ColumnDef>(0, MakeShared<DataType>(LogicalType::kInteger), "OK", HashSet<ConstraintType>()),
            };
            break;
        }
        default: {
            Error<NotImplementException>(Format("{} isn't supported here.", PhysicalOperatorToString(task_operator_state->operator_type_)));
        }
    }
}

void PhysicalSink::FillSinkStateFromLastOperatorState(MessageSinkState *message_sink_state, OperatorState *task_operator_state) {
    switch (task_operator_state->operator_type_) {
        case PhysicalOperatorType::kImport: {
            auto *import_output_state = static_cast<ImportOperatorState *>(task_operator_state);
            message_sink_state->message_ = Move(import_output_state->result_msg_);
            break;
        }
        case PhysicalOperatorType::kInsert: {
            auto *insert_output_state = static_cast<InsertOperatorState *>(task_operator_state);
            message_sink_state->message_ = Move(insert_output_state->result_msg_);
            break;
        }
        default: {
            Error<NotImplementException>(Format("{} isn't supported here.", PhysicalOperatorToString(task_operator_state->operator_type_)));
            break;
        }
    }
}

void PhysicalSink::FillSinkStateFromLastOperatorState(QueueSinkState *queue_sink_state, OperatorState *task_operator_state) {
    if (queue_sink_state->error_message_.get() != nullptr) {
        LOG_TRACE(Format("Error: {} is sent to notify next fragment", *queue_sink_state->error_message_));
        auto fragment_error = MakeShared<FragmentError>(queue_sink_state->fragment_id_, MakeUnique<String>(*queue_sink_state->error_message_));
        for (const auto &next_fragment_queue : queue_sink_state->fragment_data_queues_) {
            next_fragment_queue->Enqueue(fragment_error);
        }
        return;
    }

    if (!task_operator_state->Complete()) {
        LOG_TRACE("Task not completed");
        return;
    }
    SizeT output_data_block_count = task_operator_state->data_block_array_.size();
    if (output_data_block_count == 0) {
        for (const auto &next_fragment_queue : queue_sink_state->fragment_data_queues_) {
            next_fragment_queue->Enqueue(MakeShared<FragmentNone>(queue_sink_state->fragment_id_));
        }
        return;
        // Error<ExecutorException>("No output from last operator.");
    }
    for (SizeT idx = 0; idx < output_data_block_count; ++idx) {
        auto fragment_data = MakeShared<FragmentData>(queue_sink_state->fragment_id_,
                                                      Move(task_operator_state->data_block_array_[idx]),
                                                      queue_sink_state->task_id_,
                                                      idx,
                                                      output_data_block_count);
        for (const auto &next_fragment_queue : queue_sink_state->fragment_data_queues_) {
            next_fragment_queue->Enqueue(fragment_data);
        }
    }
    task_operator_state->data_block_array_.clear();
}

} // namespace infinity
